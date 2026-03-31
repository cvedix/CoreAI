#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include "cvedix/nodes/broker/cvedix_json_enhanced_console_broker_node.h"
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include "cvedix/nodes/broker/cereal_archive/cvedix_objects_cereal_archive.h"
#include "cpp_base64/base64.h"

#include <set>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <chrono>

/*
* ## ba crossline mqtt sample ##
* behaviour analysis for crossline with MQTT event reporting.
*/

// Helper functions
namespace {
    std::string get_current_timestamp() {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return std::to_string(ms);
    }
    
    std::string get_current_date_iso() {
        auto now = std::time(nullptr);
        std::tm tm;
        gmtime_r(&now, &tm);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return std::string(buf);
    }
    
    std::string get_current_date_system() {
        auto now = std::time(nullptr);
        std::tm tm;
        localtime_r(&now, &tm);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tm);
        return std::string(buf);
    }
}

// Structures for event format
namespace event_format {
    struct normalized_bbox {
        double x, y, width, height;
        
        template<typename Archive>
        void serialize(Archive& archive) {
            archive(cereal::make_nvp("x", x),
                    cereal::make_nvp("y", y),
                    cereal::make_nvp("width", width),
                    cereal::make_nvp("height", height));
        }
    };
    
    struct track_info {
        normalized_bbox bbox;
        std::string class_label;
        std::string external_id;
        std::string id;
        int last_seen;
        int source_tracker_track_id;
        
        template<typename Archive>
        void serialize(Archive& archive) {
            archive(cereal::make_nvp("bbox", bbox),
                    cereal::make_nvp("class_label", class_label),
                    cereal::make_nvp("external_id", external_id),
                    cereal::make_nvp("id", id),
                    cereal::make_nvp("last_seen", last_seen),
                    cereal::make_nvp("source_tracker_track_id", source_tracker_track_id));
        }
    };
    
    struct best_thumbnail {
        double confidence;
        std::string image;
        std::string instance_id;
        std::string label;
        std::string system_date;
        std::vector<track_info> tracks;
        
        template<typename Archive>
        void serialize(Archive& archive) {
            archive(cereal::make_nvp("confidence", confidence),
                    cereal::make_nvp("image", image),
                    cereal::make_nvp("instance_id", instance_id),
                    cereal::make_nvp("label", label),
                    cereal::make_nvp("system_date", system_date),
                    cereal::make_nvp("tracks", tracks));
        }
    };
    
    struct event {
        best_thumbnail best_thumbnail_obj;
        std::string type;
        std::string zone_id;
        std::string zone_name;
        
        template<typename Archive>
        void serialize(Archive& archive) {
            archive(cereal::make_nvp("best_thumbnail", best_thumbnail_obj),
                    cereal::make_nvp("type", type),
                    cereal::make_nvp("zone_id", zone_id),
                    cereal::make_nvp("zone_name", zone_name));
        }
    };
    
    struct event_message {
        std::vector<event> events;
        int frame_id;
        double frame_time;
        std::string system_date;
        std::string system_timestamp;
        
        template<typename Archive>
        void serialize(Archive& archive) {
            archive(cereal::make_nvp("events", events),
                    cereal::make_nvp("frame_id", frame_id),
                    cereal::make_nvp("frame_time", frame_time),
                    cereal::make_nvp("system_date", system_date),
                    cereal::make_nvp("system_timestamp", system_timestamp));
        }
    };
}

// Custom Broker Node for Crossline MQTT
class cvedix_json_crossline_mqtt_broker_node : public cvedix_nodes::cvedix_json_enhanced_console_broker_node {
private:
    std::function<void(const std::string&)> mqtt_publisher_;
    std::string instance_id_;
    std::string zone_id_;
    std::string zone_name_;
    
    virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override {
        try {
            if (meta->ba_results.empty()) {
                msg = "";
                return;
            }

            event_format::event_message event_msg;
            bool has_events = false;

            double frame_width = static_cast<double>(meta->frame.cols);
            double frame_height = static_cast<double>(meta->frame.rows);

            for (const auto& ba_res : meta->ba_results) {
                // Check if it's a crossline event
                if (ba_res->type == cvedix_objects::cvedix_ba_type::CROSSLINE) {
                    
                    // Iterate over all targets involved in this crossline event
                    for (int track_id : ba_res->involve_target_ids_in_frame) {
                        // Find the target object
                        std::shared_ptr<cvedix_objects::cvedix_frame_target> target = nullptr;
                        for (const auto& t : meta->targets) {
                            if (t->track_id == track_id) {
                                target = t;
                                break;
                            }
                        }

                        if (!target) continue;
                        
                        has_events = true;
                        event_format::event evt;
                        
                        // Crop image logic
                        std::string thumbnail_image = "";
                        try {
                            if (!meta->frame.empty()) {
                                // Optimization: Use const reference instead of clone
                                const cv::Mat& original_frame = meta->frame;
                                
                                const float expand_ratio = 0.35f;
                                int center_x = target->x + target->width / 2;
                                int center_y = target->y + target->height / 2;
                                
                                int expanded_width = static_cast<int>(target->width * (1.0f + expand_ratio));
                                int expanded_height = static_cast<int>(target->height * (1.0f + expand_ratio));
                                
                                int x1 = std::max(0, center_x - expanded_width / 2);
                                int y1 = std::max(0, center_y - expanded_height / 2);
                                int x2 = std::min(original_frame.cols, x1 + expanded_width);
                                int y2 = std::min(original_frame.rows, y1 + expanded_height);
                                
                                if (x2 > x1 && y2 > y1) {
                                    cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
                                    // Optimization: Create ROI (O(1)) instead of clone
                                    cv::Mat cropped = original_frame(roi);
                                    
                                    if (!cropped.empty()) {
                                        cv::Mat resized;
                                        cv::resize(cropped, resized, cv::Size(150, 150), 0, 0, cv::INTER_LINEAR);
                                        std::vector<uchar> buf;
                                        cv::imencode(".jpg", resized, buf);
                                        thumbnail_image = base64_encode(buf.data(), buf.size());
                                    }
                                }
                            }
                        } catch (...) {
                            thumbnail_image = "";
                        }

                        // Track info
                        event_format::track_info track;
                        track.bbox.x = target->x / frame_width;
                        track.bbox.y = target->y / frame_height;
                        track.bbox.width = target->width / frame_width;
                        track.bbox.height = target->height / frame_height;
                        
                        track.class_label = target->primary_label.empty() ? "Object" : target->primary_label;
                        track.external_id = "crossline-event"; // Or generate UUID
                        track.id = "Tracker_" + std::to_string(target->track_id);
                        track.last_seen = 0;
                        track.source_tracker_track_id = target->track_id;

                        evt.best_thumbnail_obj.confidence = target->primary_score;
                        evt.best_thumbnail_obj.image = thumbnail_image;
                        evt.best_thumbnail_obj.instance_id = instance_id_;
                        evt.best_thumbnail_obj.label = ba_res->ba_label; // "cross line"
                        evt.best_thumbnail_obj.system_date = get_current_date_iso();
                        evt.best_thumbnail_obj.tracks.push_back(track);

                        evt.type = "crossline";
                        evt.zone_id = zone_id_;
                        evt.zone_name = zone_name_;

                        event_msg.events.push_back(evt);
                    }
                }
            }

            if (!has_events) {
                msg = "";
                return;
            }

            event_msg.frame_id = meta->frame_index;
            event_msg.frame_time = meta->frame_index * 1000.0 / (meta->fps > 0 ? meta->fps : 30.0);
            event_msg.system_date = get_current_date_system();
            event_msg.system_timestamp = get_current_timestamp();

            std::stringstream msg_stream;
            {
                cereal::JSONOutputArchive json_archive(msg_stream);
                std::vector<event_format::event_message> result_array;
                result_array.push_back(event_msg);
                json_archive(result_array);
            }
            
             // Clean up JSON wrapper
            std::string json_str = msg_stream.str();
            size_t value0_pos = json_str.find("\"value0\"");
            if (value0_pos != std::string::npos) {
                size_t array_start = json_str.find('[', value0_pos);
                if (array_start != std::string::npos) {
                    size_t array_end = json_str.rfind(']');
                    if (array_end != std::string::npos && array_end > array_start) {
                        msg = json_str.substr(array_start, array_end - array_start + 1);
                    } else {
                        msg = json_str;
                    }
                } else {
                    msg = json_str;
                }
            } else {
                msg = json_str;
            }

        } catch (...) {
            msg = "";
        }
    }

    virtual void broke_msg(const std::string& msg) override {
        if (mqtt_publisher_ && !msg.empty()) {
            try {
                mqtt_publisher_(msg);
            } catch (...) {
                // Silent fail or minimal log
            }
        }
    }

public:
    cvedix_json_crossline_mqtt_broker_node(
        std::string node_name,
        std::function<void(const std::string&)> mqtt_publisher,
        std::string instance_id = "DEMO",
        std::string zone_id = "default_zone",
        std::string zone_name = "CrosslineZone")
        : cvedix_nodes::cvedix_json_enhanced_console_broker_node(
            node_name, cvedix_nodes::cvedix_broke_for::NORMAL, 100, 500, false)
        , mqtt_publisher_(mqtt_publisher)
        , instance_id_(instance_id)
        , zone_id_(zone_id)
        , zone_name_(zone_name)
    {
    }
};

// Global MQTT variables
static std::unique_ptr<cvedix_utils::cvedix_mqtt_client> g_mqtt_publisher = nullptr;
static std::mutex g_mqtt_publish_mutex;
static std::string g_mqtt_publish_topic = "events";

void mqtt_publish_crossline_data(const std::string& json_message) {
    std::lock_guard<std::mutex> lock(g_mqtt_publish_mutex);
    if (g_mqtt_publisher && g_mqtt_publisher->is_ready()) {
         g_mqtt_publisher->publish(g_mqtt_publish_topic, json_message, 1, false);
         std::cout << "[MQTT] Published crossline event." << std::endl;
    }
}

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // MQTT Configuration
    std::string mqtt_broker = "anhoidong.datacenter.cvedix.com";
    int mqtt_port = 1883;
    std::string mqtt_topic = "events";
    
    // Simple args parsing for MQTT: [broker] [port] [topic]
    if (argc > 1) mqtt_broker = argv[1];
    if (argc > 2) mqtt_port = std::stoi(argv[2]);
    if (argc > 3) mqtt_topic = argv[3];

    g_mqtt_publish_topic = mqtt_topic;
    
    std::cout << "[Main] Connecting to MQTT Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
    g_mqtt_publisher = std::make_unique<cvedix_utils::cvedix_mqtt_client>(
        mqtt_broker, mqtt_port, "crossline_sample_" + std::to_string(std::time(nullptr)), 60);
    g_mqtt_publisher->set_auto_reconnect(true, 5000);
    g_mqtt_publisher->connect("", "");

    // create nodes
    // Using file source for consistency with original sample
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/vehicle_count.mp4", 1.0);
    
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector",                              // node name
        "./cvedix_data/models/yolov11/onnx/yolo11n.onnx", // ONNX engine file
        "./cvedix_data/models/yolov11/onnx/labels.txt",  // labels file
        0.45,   // confidence threshold
        0.5,     // NMS threshold
        0,
        cvedix_nodes::BackendType::ONNX
    );
    
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");
    
    // define a line in frame for every channel (value MUST in the scope of frame'size)
    cvedix_objects::cvedix_point start(0, 250);  // change to proper value
    cvedix_objects::cvedix_point end(700, 220);  // change to proper value
    std::map<int, cvedix_objects::cvedix_line> lines = {{0, cvedix_objects::cvedix_line(start, end)}};  // channel0 -> line
    
    auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>("ba_crossline", lines);
    
    auto mqtt_broker_node = std::make_shared<cvedix_json_crossline_mqtt_broker_node>(
        "mqtt_broker_node", mqtt_publish_crossline_data);

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    
    // construct pipeline
    yolo_detector->attach_to({file_src_0});
    tracker->attach_to({yolo_detector});
    ba_crossline->attach_to({tracker});

    mqtt_broker_node->attach_to({ba_crossline});
    osd->attach_to({mqtt_broker_node});

    screen_des_0->attach_to({osd});

    file_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    
    if (g_mqtt_publisher) {
        g_mqtt_publisher->disconnect();
    }

    return 0;
}
