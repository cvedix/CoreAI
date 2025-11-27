#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/des/cvedix_fake_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_custom_data_transform_node.h"
#ifdef CVEDIX_WITH_MQTT
#include "cvedix/nodes/broker/cvedix_json_mqtt_broker_node.h"
#include "cvedix/nodes/broker/cvedix_json_enhanced_console_broker_node.h"
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#endif

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#ifdef CVEDIX_WITH_MQTT
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#endif
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <ctime>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <set>
#include <opencv2/imgcodecs.hpp>
#include "cvedix/nodes/broker/cereal_archive/cvedix_objects_cereal_archive.h"
#include "cpp_base64/base64.h"

#ifdef CVEDIX_WITH_MQTT
// Helper functions for new format
namespace {
    std::string mat_to_base64(const cv::Mat& img, const std::string& ext = ".jpg") {
        if (img.empty()) {
            return "";
        }
        std::vector<uchar> buf;
        cv::imencode(ext, img, buf);
        std::string encoded = base64_encode(buf.data(), buf.size());
        return encoded;
    }
    
    cv::Mat crop_image(const cv::Mat& frame, int x, int y, int width, int height) {
        if (frame.empty()) {
            return cv::Mat();
        }
        int x1 = std::max(0, x);
        int y1 = std::max(0, y);
        int x2 = std::min(frame.cols, x + width);
        int y2 = std::min(frame.rows, y + height);
        if (x2 <= x1 || y2 <= y1) {
            return cv::Mat();
        }
        cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
        return frame(roi).clone();
    }
    
    std::string get_current_timestamp() {
        auto now = std::time(nullptr);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return std::to_string(ms);
    }
    
    std::string get_current_date_iso() {
        auto now = std::time(nullptr);
        auto tm = *std::gmtime(&now);
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
    
    std::string get_current_date_system() {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::stringstream ss;
        ss << std::put_time(&tm, "%a %b %d %H:%M:%S %Y");
        return ss.str();
    }
}

// Structures for new event-based format
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

// Custom Enhanced MQTT Broker Node: Kế thừa từ enhanced broker và gửi qua MQTT với format mới
class cvedix_json_enhanced_mqtt_broker_node : public cvedix_nodes::cvedix_json_enhanced_console_broker_node {
private:
    std::function<void(const std::string&)> mqtt_publisher_;
    std::string instance_id_;
    std::string zone_id_;
    std::string zone_name_;
    // Cache các track_id đã gửi để chỉ gửi event mới
    std::set<int> sent_track_ids_;
    std::mutex sent_tracks_mutex_;
    
    // Override format_msg để tạo format mới
    virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override {
        try {
            event_format::event_message event_msg;
            
            // Tìm target có confidence cao nhất cho best_thumbnail
            std::shared_ptr<cvedix_objects::cvedix_frame_target> best_target = nullptr;
            float best_confidence = 0.0f;
            
            for (const auto& target : meta->targets) {
                if (target->primary_score > best_confidence) {
                    best_confidence = target->primary_score;
                    best_target = target;
                }
            }
            
            // Kiểm tra xem có track_id mới nào chưa được gửi không
            std::set<int> current_track_ids;
            for (const auto& target : meta->targets) {
                if (target->track_id >= 0) {
                    current_track_ids.insert(target->track_id);
                }
            }
            
            // Tìm các track_id mới (chưa được gửi)
            std::set<int> new_track_ids;
            {
                std::lock_guard<std::mutex> lock(sent_tracks_mutex_);
                for (int track_id : current_track_ids) {
                    if (sent_track_ids_.find(track_id) == sent_track_ids_.end()) {
                        new_track_ids.insert(track_id);
                        sent_track_ids_.insert(track_id);  // Đánh dấu đã gửi
                    }
                }
            }
            
            // Tạo một event cho mỗi track mới xuất hiện (để nhận diện nhiều khuôn mặt cùng lúc)
            if (!new_track_ids.empty() && !meta->targets.empty()) {
                double frame_width = static_cast<double>(meta->frame.cols);
                double frame_height = static_cast<double>(meta->frame.rows);
                
                // Tạo event cho mỗi track mới
                for (int new_track_id : new_track_ids) {
                    // Tìm target tương ứng với track_id này
                    std::shared_ptr<cvedix_objects::cvedix_frame_target> target_for_track = nullptr;
                    for (const auto& target : meta->targets) {
                        if (target->track_id == new_track_id) {
                            target_for_track = target;
                            break;
                        }
                    }
                    
                    if (!target_for_track) {
                        continue;  // Bỏ qua nếu không tìm thấy target
                    }
                    
                    event_format::event evt;
                    
                    // Crop và encode thumbnail cho track này
                    std::string thumbnail_image = "";
                    try {
                        // Crop ảnh từ frame gốc (không có bounding box) và resize về 150x150
                        // Clone toàn bộ frame trước để đảm bảo không bị ảnh hưởng bởi các node khác
                        cv::Mat cropped;
                        if (!meta->frame.empty()) {
                            // Clone frame gốc để đảm bảo không có bounding box
                            cv::Mat original_frame = meta->frame.clone();
                            
                            // Tính toán bbox mở rộng thêm 35%
                            const float expand_ratio = 0.35f;  // Mở rộng 35%
                            
                            // Tính center của bbox gốc
                            int center_x = target_for_track->x + target_for_track->width / 2;
                            int center_y = target_for_track->y + target_for_track->height / 2;
                            
                            // Mở rộng width và height
                            int expanded_width = static_cast<int>(target_for_track->width * (1.0f + expand_ratio));
                            int expanded_height = static_cast<int>(target_for_track->height * (1.0f + expand_ratio));
                            
                            // Tính toán x1, y1 để giữ center ở giữa
                            int x1 = center_x - expanded_width / 2;
                            int y1 = center_y - expanded_height / 2;
                            int x2 = x1 + expanded_width;
                            int y2 = y1 + expanded_height;
                            
                            // Đảm bảo không vượt quá biên frame
                            x1 = std::max(0, x1);
                            y1 = std::max(0, y1);
                            x2 = std::min(original_frame.cols, x2);
                            y2 = std::min(original_frame.rows, y2);
                            
                            // Đảm bảo width và height hợp lệ
                            if (x2 > x1 && y2 > y1) {
                                // Crop từ frame gốc đã clone (không có bounding box)
                                cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
                                cropped = original_frame(roi).clone();
                                
                                // Resize về 150x150
                                if (!cropped.empty()) {
                                    cv::Mat resized;
                                    cv::resize(cropped, resized, cv::Size(150, 150), 0, 0, cv::INTER_LINEAR);
                                    cropped = resized;
                                }
                            }
                        }
                        if (!cropped.empty()) {
                            std::vector<uchar> buf;
                            cv::imencode(".jpg", cropped, buf);
                            thumbnail_image = base64_encode(buf.data(), buf.size());
                        }
                    } catch (...) {
                        thumbnail_image = "";
                    }
                    
                    // Tạo track info cho track này
                    event_format::track_info track;
                    track.bbox.x = target_for_track->x / frame_width;
                    track.bbox.y = target_for_track->y / frame_height;
                    track.bbox.width = target_for_track->width / frame_width;
                    track.bbox.height = target_for_track->height / frame_height;
                    
                    track.class_label = target_for_track->primary_label.empty() ? "Person" : target_for_track->primary_label;
                    track.external_id = "a42f6aa6-637b-419f-a2dd-f036454a8cd5";  // Có thể generate UUID thực tế
                    track.id = "PersonTracker_" + std::to_string(target_for_track->track_id);
                    track.last_seen = 0;
                    track.source_tracker_track_id = target_for_track->track_id;
                    
                    // Fill best_thumbnail cho event này
                    evt.best_thumbnail_obj.confidence = target_for_track->primary_score;
                    evt.best_thumbnail_obj.image = thumbnail_image;
                    evt.best_thumbnail_obj.instance_id = instance_id_;
                    evt.best_thumbnail_obj.label = "Entered area";
                    evt.best_thumbnail_obj.system_date = get_current_date_iso();
                    evt.best_thumbnail_obj.tracks.push_back(track);  // Chỉ có track này trong tracks array
                    
                    // Fill event
                    evt.type = "area_enter";
                    evt.zone_id = zone_id_;
                    evt.zone_name = zone_name_;
                    
                    event_msg.events.push_back(evt);
                }
            }
            
            // Chỉ serialize và trả về nếu có events
            if (event_msg.events.empty()) {
                msg = "";  // Không trả về gì nếu không có events
                return;
            }
            
            // Fill root level
            event_msg.frame_id = meta->frame_index;
            event_msg.frame_time = meta->frame_index * 1000.0 / (meta->fps > 0 ? meta->fps : 30.0);
            event_msg.system_date = get_current_date_system();
            event_msg.system_timestamp = get_current_timestamp();
            
            // Serialize to JSON - tạo mảng chứa một object [{...}] và bỏ wrapper "value0"
            std::stringstream msg_stream;
            {
                cereal::JSONOutputArchive json_archive(msg_stream);
                // Tạo vector chứa một event_message để serialize thành mảng
                std::vector<event_format::event_message> result_array;
                result_array.push_back(event_msg);
                json_archive(result_array);
            }
            
            // Bỏ wrapper "value0" từ JSON string
            std::string json_str = msg_stream.str();
            // Tìm và bỏ phần "{\n    \"value0\": " ở đầu và "}" ở cuối
            size_t value0_pos = json_str.find("\"value0\"");
            if (value0_pos != std::string::npos) {
                // Tìm vị trí bắt đầu của mảng (sau "value0": )
                size_t array_start = json_str.find('[', value0_pos);
                if (array_start != std::string::npos) {
                    // Tìm vị trí kết thúc của mảng (trước "}" cuối cùng)
                    size_t array_end = json_str.rfind(']');
                    if (array_end != std::string::npos && array_end > array_start) {
                        // Lấy phần mảng và thêm newline nếu cần
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
        } catch (const std::exception& e) {
            msg = "";
        } catch (...) {
            msg = "";
        }
    }

protected:
    // Override broke_msg để gửi qua MQTT thay vì console
    virtual void broke_msg(const std::string& msg) override {
        if (mqtt_publisher_ && !msg.empty()) {
            try {
                mqtt_publisher_(msg);
            } catch (const std::exception& e) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] MQTT publish failed: %s", 
                    node_name.c_str(), e.what()));
            } catch (...) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] MQTT publish failed with unknown error", 
                    node_name.c_str()));
            }
        }
    }

public:
    cvedix_json_enhanced_mqtt_broker_node(
        std::string node_name,
        cvedix_nodes::cvedix_broke_for broke_for,
        int broking_cache_warn_threshold,
        int broking_cache_ignore_threshold,
        bool encode_full_frame,
        std::function<void(const std::string&)> mqtt_publisher,
        std::string instance_id = "DEMO",
        std::string zone_id = "95493308-c879-4f85-9fb7-36433971f60c",
        std::string zone_name = "Quan Giao")
        : cvedix_nodes::cvedix_json_enhanced_console_broker_node(
            node_name, broke_for, broking_cache_warn_threshold, 
            broking_cache_ignore_threshold, encode_full_frame)
        , mqtt_publisher_(mqtt_publisher)
        , instance_id_(instance_id)
        , zone_id_(zone_id)
        , zone_name_(zone_name)
    {
    }
    
    ~cvedix_json_enhanced_mqtt_broker_node() = default;
    
    void set_mqtt_publisher(std::function<void(const std::string&)> publisher) {
        mqtt_publisher_ = publisher;
    }
    
    void set_zone_info(const std::string& zone_id, const std::string& zone_name) {
        zone_id_ = zone_id;
        zone_name_ = zone_name;
    }
    
    void set_instance_id(const std::string& instance_id) {
        instance_id_ = instance_id;
    }
};
#endif

/*
* ## Ví dụ theo dõi đối tượng qua RTSP dùng RKNN với MQTT ##
* Đầu vào RTSP → Bộ phát hiện RKNN YOLOv8 → Bộ theo dõi SORT → 
* OSD → Hiển thị màn hình / RTMP streaming
*      └─> MQTT Broker → Gửi dữ liệu tracking lên MQTT
*
* Tính năng:
* - Phát hiện và theo dõi đối tượng sử dụng RKNN YOLOv8
* - Hiển thị kết quả lên màn hình
* - Streaming lên RTMP server
* - Gửi dữ liệu JSON từ tracker (có crop images) lên MQTT broker
*
* Yêu cầu:
* - Model RKNN (.rknn)
* - libmosquitto-dev (cho MQTT support)
*
* Biên dịch:
*   cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_WITH_RTSP_SERVER=ON -DCVEDIX_WITH_MQTT=ON ..
*
* Sử dụng:
*   ./rknn_rtsp_tracking_mqtt_sample [model_path] [rtsp_url] [mqtt_broker] [mqtt_port] [mqtt_topic] [username] [password]
*
* Ví dụ:
*   ./rknn_rtsp_tracking_mqtt_sample model.rknn rtsp://... anhoidong.datacenter.cvedix.com 1883 events
*/

// Cờ toàn cục để xử lý tín hiệu
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

#ifdef CVEDIX_WITH_MQTT
// Global MQTT client instance (để publish dữ liệu từ tracker)
static std::unique_ptr<cvedix_utils::cvedix_mqtt_client> g_mqtt_publisher = nullptr;
static std::mutex g_mqtt_publish_mutex;
static std::string g_mqtt_publish_topic = "events";
static int g_mqtt_publish_count = 0;

// MQTT publisher function để gửi JSON từ tracker
void mqtt_publish_tracking_data(const std::string& json_message) {
    std::lock_guard<std::mutex> lock(g_mqtt_publish_mutex);
    
    if (!g_mqtt_publisher || !g_mqtt_publisher->is_ready()) {
        // Silent fail - không log để tránh spam
        return;
    }
    
    try {
        int mid = g_mqtt_publisher->publish(g_mqtt_publish_topic, json_message, 1, false);
        if (mid >= 0) {
            g_mqtt_publish_count++;
            // Log mỗi 100 messages
            if (g_mqtt_publish_count % 100 == 0) {
                std::cout << "[MQTT Publisher] Published " << g_mqtt_publish_count 
                          << " tracking messages to topic: " << g_mqtt_publish_topic << std::endl;
            }
        }
    } catch (const std::exception& e) {
        // Silent fail
    }
}
#endif

int main(int argc, char** argv) {
    // Xử lý Ctrl+C
    std::signal(SIGINT, signal_handler);

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // Đường dẫn model mặc định
    std::string model_path = "./cvedix_data/models/face/yolov8n_face_detection.rknn";
    if (argc > 1) {
        model_path = argv[1];
    }
    
    std::string rtsp_url = "rtsp://103.147.186.175:18554/livestream/5F0459EPAG1EB5A";
    if (argc > 2) {
        rtsp_url = argv[2];
    }
    
#ifdef CVEDIX_WITH_MQTT
    // MQTT configuration
    std::string mqtt_broker = "anhoidong.datacenter.cvedix.com";
    int mqtt_port = 1883;
    std::string mqtt_topic = "events";  // Topic để publish dữ liệu tracking
    std::string mqtt_username = "";
    std::string mqtt_password = "";
    g_mqtt_publish_topic = mqtt_topic;
    
    // Initialize MQTT publisher (để gửi dữ liệu tracking)
    std::cout << "[Main] Initializing MQTT publisher..." << std::endl;
    std::cout << "[Main] Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
    std::cout << "[Main] Topic: " << mqtt_topic << std::endl;
    
    g_mqtt_publisher = std::make_unique<cvedix_utils::cvedix_mqtt_client>(
        mqtt_broker,
        mqtt_port,
        "rknn_tracking_publisher_" + std::to_string(std::time(nullptr)),
        60
    );
    
    g_mqtt_publisher->set_auto_reconnect(true, 5000);
    
    if (!g_mqtt_publisher->connect(mqtt_username, mqtt_password)) {
        std::cerr << "[Main] Warning: MQTT publisher connection failed: " << g_mqtt_publisher->get_last_error() << std::endl;
        std::cerr << "[Main] Continuing without MQTT publisher..." << std::endl;
    } else {
        int retry_count = 0;
        while (!g_mqtt_publisher->is_connected() && retry_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            retry_count++;
        }
        if (g_mqtt_publisher->is_connected()) {
            std::cout << "[Main] MQTT publisher connected successfully!" << std::endl;
        }
    }
#endif

    // Đầu vào: Nguồn RTSP
    // resize_ratio: 1.0 để giữ nguyên độ phân giải gốc (tốt hơn cho detection)
    // Nếu muốn tăng tốc có thể giảm xuống 0.8-0.9 nhưng sẽ giảm độ chính xác
    auto rtsp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src_0", 0, rtsp_url, 0.9, "mppvideodec", 0, "auto");
    
    // Suy luận: Phát hiện khuôn mặt sử dụng RKNN YOLOv8
    // Cải thiện hiệu suất và độ chính xác:
    // - Input size: 640x640 (tốt hơn 320x320 cho face detection)
    // - Score threshold: 0.5 (tăng từ 0.25 để giảm false positives)
    // - NMS threshold: 0.45 (tối ưu cho face detection)
    auto rknn_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
        "rknn_detector_0", 
        model_path,
        0.5,   // ngưỡng điểm (tăng từ 0.25 để giảm false positives và tăng tốc)
        0.45,  // ngưỡng NMS (tối ưu cho face detection)
        640,   // chiều rộng đầu vào (tăng từ 320 để tăng độ chính xác)
        640,   // chiều cao đầu vào (tăng từ 320 để tăng độ chính xác)
        1      // số lớp (face detection model chỉ có 1 class)
    );

    // Theo dõi: Bộ theo dõi SORT
    auto sort_tracker_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_tracker_0", 
        cvedix_nodes::cvedix_track_for::NORMAL
    );
    
    // OSD: Vẽ kết quả lên khung hình
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");

    // Split node: Chia luồng thành 2 nhánh chính (RTMP và Screen)
    auto split_node_0 = std::make_shared<cvedix_nodes::cvedix_split_node>("split_node_0");
    
#ifdef CVEDIX_WITH_MQTT
    // Custom Data Transform Node: Tùy chỉnh dữ liệu theo yêu cầu khách hàng
    // Ví dụ: Filter targets, thêm/sửa thông tin, transform dữ liệu
    auto custom_transform_0 = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
        "custom_transform_0",
        [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) -> std::shared_ptr<cvedix_objects::cvedix_frame_meta> {
            if (!meta || meta->targets.empty()) {
                return meta; // Forward as-is if no targets
            }
            
            // Ví dụ 1: Filter targets với score >= 0.3 (loại bỏ targets có score thấp)
            // Comment out để không filter, cho phép tất cả targets đi qua
            // auto it = meta->targets.begin();
            // while (it != meta->targets.end()) {
            //     if ((*it)->primary_score < 0.3f) {
            //         it = meta->targets.erase(it);
            //     } else {
            //         ++it;
            //     }
            // }
            
            // Không filter gì cả - để tất cả targets đi qua để nhận diện nhiều khuôn mặt
            
            // Ví dụ 2: Thêm custom label nếu cần
            // for (auto& target : meta->targets) {
            //     if (target->primary_class_id == 0 && target->primary_score > 0.5f) {
            //         target->primary_label = "HighConfidencePerson";
            //     }
            // }
            
            // Ví dụ 3: Filter theo track_id (chỉ giữ targets đã được track)
            // auto it = meta->targets.begin();
            // while (it != meta->targets.end()) {
            //     if ((*it)->track_id < 0) {  // track_id = -1 means not tracked
            //         it = meta->targets.erase(it);
            //     } else {
            //         ++it;
            //     }
            // }
            
            // Ví dụ 4: Thêm custom metadata vào description
            // if (!meta->targets.empty()) {
            //     meta->description = "Detected " + std::to_string(meta->targets.size()) + " objects";
            // }
            
            // Return modified meta (hoặc nullptr để drop frame này)
            return meta;
        }
    );
    
    // Enhanced MQTT Broker: Tạo JSON với base64 crop images và gửi qua MQTT
    // Chạy nối tiếp trong pipeline: Tracker → Custom Transform → MQTT Broker → OSD
    auto enhanced_mqtt_broker_0 = std::make_shared<cvedix_json_enhanced_mqtt_broker_node>(
        "enhanced_mqtt_broker_0",
        cvedix_nodes::cvedix_broke_for::NORMAL,
        100,  // broking_cache_warn_threshold
        500,  // broking_cache_ignore_threshold
        false, // encode_full_frame (tắt để tiết kiệm memory, chỉ encode crop images)
        mqtt_publish_tracking_data  // MQTT publisher function
    );
#endif
    
    // Đầu ra: Hiển thị lên màn hình
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Đầu ra: Gửi lên RTMP server
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://anhoidong.datacenter.cvedix.com:1935/live/2000", cvedix_objects::cvedix_size{1280, 720}, 1024 * 2);

    // Xây dựng pipeline
    rknn_detector_0->attach_to({rtsp_src_0});
    sort_tracker_0->attach_to({rknn_detector_0});
    
#ifdef CVEDIX_WITH_MQTT
    // Pipeline với Custom Transform: Tracker → Custom Transform → MQTT Broker → OSD
    // Custom Transform: Tùy chỉnh dữ liệu (filter, transform) trước khi gửi qua MQTT
    custom_transform_0->attach_to({sort_tracker_0});
    enhanced_mqtt_broker_0->attach_to({custom_transform_0});
    osd_0->attach_to({enhanced_mqtt_broker_0});
#else
    // OSD nhận trực tiếp từ tracker (khi không có MQTT)
    osd_0->attach_to({sort_tracker_0});
#endif
    
    // Split node: Chia luồng thành 2 nhánh chính (RTMP và Screen)
    split_node_0->attach_to({osd_0});
    rtmp_des_0->attach_to({split_node_0});
    screen_des_0->attach_to({split_node_0});

    // Khởi động pipeline
    rtsp_src_0->start();

    // Bảng phân tích (GUI)
    cvedix_utils::cvedix_analysis_board board({rtsp_src_0});
    board.display(1, false);

    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Dọn dẹp
    rtsp_src_0->detach_recursively();
    
#ifdef CVEDIX_WITH_MQTT
    // Cleanup MQTT publisher
    if (g_mqtt_publisher) {
        std::cout << "[Main] Disconnecting MQTT publisher..." << std::endl;
        std::cout << "[Main] Total published messages: " << g_mqtt_publish_count << std::endl;
        g_mqtt_publisher->disconnect();
        g_mqtt_publisher.reset();
    }
#endif
    
    return 0;
}


