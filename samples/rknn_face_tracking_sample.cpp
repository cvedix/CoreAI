#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_face_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/des/cvedix_fake_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_custom_data_transform_node.h"
#include "cvedix/nodes/broker/cvedix_json_enhanced_console_broker_node.h"
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include "cvedix/nodes/broker/cereal_archive/cvedix_objects_cereal_archive.h"
#include "cpp_base64/base64.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <thread>
#include <chrono>
#include <iostream>
#include <memory>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <mutex>
#include <opencv2/imgcodecs.hpp>

/*
* ## Ví dụ phát hiện và theo dõi khuôn mặt sử dụng RKNN NPU với MQTT ##
* Đầu vào RTSP → Bộ phát hiện RKNN Face Detector → Filter → Bộ theo dõi SORT → 
* MQTT Broker → OSD → Split → [Screen Display | RTMP Streaming]
*
* Tính năng:
* - Phát hiện khuôn mặt sử dụng RKNN NPU (tối ưu cho Rockchip hardware)
* - Sử dụng model YuNet (libfacedetection) đã được convert sang RKNN format
* - Hỗ trợ phát hiện 5 keypoints (mắt trái, mắt phải, mũi, góc miệng trái, góc miệng phải)
* - Tự động sử dụng RGA (Rockchip Graphics Accelerator) nếu có để tăng tốc pre-processing
* - Theo dõi nhiều khuôn mặt cùng lúc với SORT tracker
* - Filter theo kích thước khuôn mặt để loại bỏ false positives
* - Gửi events qua MQTT khi có khuôn mặt mới xuất hiện (với crop ảnh)
* - Hỗ trợ RTMP streaming output
* - Hỗ trợ screen display (tự động fallback sang fake_des nếu không có DISPLAY)
*
* Yêu cầu:
* - Model RKNN (.rknn) cho face detection (YuNet format)
*   Model mặc định: ./cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn
*   Có thể convert từ ONNX model: https://github.com/ShiqiYu/libfacedetection
* - librknnrt.so (bắt buộc)
* - librga.so (tùy chọn, để tăng tốc pre-processing)
* - libmosquitto.so (cho MQTT)
*
* Biên dịch:
*   cmake -DCVEDIX_WITH_RKNN=ON [-DCVEDIX_WITH_RGA=ON] ..
*   make
*
* Sử dụng:
*   ./rknn_face_tracking_sample [model_path] [rtsp_url] [score_threshold] [nms_threshold] [top_k] [input_size] [min_face_size] [mqtt_broker] [mqtt_port] [mqtt_topic]
*
* Ví dụ:
*   # Sử dụng tham số mặc định
*   ./rknn_face_tracking_sample
*
*   # Chỉ định model và RTSP URL
*   ./rknn_face_tracking_sample ./cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn rtsp://...
*
*   # Tùy chỉnh thresholds và input size
*   ./rknn_face_tracking_sample ./model.rknn rtsp://... 0.7 0.5 50 320 40
*
*   # Độ chính xác cao (score=0.9, chỉ khuôn mặt lớn)
*   ./rknn_face_tracking_sample ./model.rknn rtsp://... 0.9 0.35 20 320 50
*
*   # Tùy chỉnh MQTT broker
*   ./rknn_face_tracking_sample ./model.rknn rtsp://... 0.7 0.5 50 320 40 14.186.188.195 30883 events
*
* Tham số:
*   - model_path: Đường dẫn đến file model RKNN (.rknn)
*   - rtsp_url: URL của RTSP stream
*   - score_threshold: Ngưỡng confidence (0.0-1.0), mặc định 0.7
*   - nms_threshold: Ngưỡng NMS để loại bỏ box trùng lặp (0.0-1.0), mặc định 0.5
*   - top_k: Số lượng khuôn mặt tối đa có thể phát hiện, mặc định 50
*   - input_size: Kích thước input cho model (width=height), mặc định 320
*   - min_face_size: Kích thước tối thiểu của khuôn mặt (pixels), mặc định 40
*   - mqtt_broker: Địa chỉ MQTT broker, mặc định "14.186.188.195"
*   - mqtt_port: Port MQTT broker, mặc định 30883
*   - mqtt_topic: Topic MQTT, mặc định "events"
*
* Output:
*   - Screen: Real-time display (nếu có DISPLAY, nếu không dùng fake_des_node)
*   - RTMP: Stream lên RTMP server (có thể comment nếu không cần)
*   - MQTT: Gửi events khi có khuôn mặt mới xuất hiện (với crop ảnh base64)
*/

// Cờ toàn cục để xử lý tín hiệu
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

// Helper functions for crop and base64 encoding
namespace {
    std::string mat_to_base64(const cv::Mat& img, const std::string& ext = ".jpg") {
        if (img.empty()) {
            return "";
        }
        std::vector<uchar> buf;
        cv::imencode(ext, img, buf);
        std::string encoded = base64_encode(buf.data(), buf.size(), false);
        return encoded;
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

// Structures for event-based format
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

// Custom Enhanced MQTT Broker Node: Crop ảnh từ frame gốc và gửi qua MQTT
class cvedix_json_enhanced_mqtt_broker_node : public cvedix_nodes::cvedix_json_enhanced_console_broker_node {
private:
    std::function<void(const std::string&)> mqtt_publisher_;
    std::string instance_id_;
    std::string zone_id_;
    std::string zone_name_;
    // Cache các track_id đã gửi để chỉ gửi event mới
    std::set<int> sent_track_ids_;
    std::mutex sent_tracks_mutex_;
    
    // Override format_msg để tạo format mới với crop ảnh
    virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override {
        try {
            event_format::event_message event_msg;
            
            // Sử dụng face_targets thay vì targets (vì đây là face tracking)
            if (meta->face_targets.empty()) {
                msg = "";
                return;
            }
            
            // Kiểm tra xem có track_id mới nào chưa được gửi không
            std::set<int> current_track_ids;
            for (const auto& face_target : meta->face_targets) {
                if (face_target->track_id >= 0) {
                    current_track_ids.insert(face_target->track_id);
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
                // Remove track_ids không còn trong frame
                std::vector<int> to_remove;
                for (int id : sent_track_ids_) {
                    if (current_track_ids.find(id) == current_track_ids.end()) {
                        to_remove.push_back(id);
                    }
                }
                for (int id : to_remove) {
                    sent_track_ids_.erase(id);
                }
            }
            
            // Tạo một event cho mỗi track mới xuất hiện
            if (!new_track_ids.empty() && !meta->face_targets.empty()) {
                double frame_width = static_cast<double>(meta->frame.cols);
                double frame_height = static_cast<double>(meta->frame.rows);
                
                // Tạo event cho mỗi track mới
                for (int new_track_id : new_track_ids) {
                    // Tìm face_target tương ứng với track_id này
                    std::shared_ptr<cvedix_objects::cvedix_frame_face_target> target_for_track = nullptr;
                    for (const auto& face_target : meta->face_targets) {
                        if (face_target->track_id == new_track_id) {
                            target_for_track = face_target;
                            break;
                        }
                    }
                    
                    if (!target_for_track) {
                        continue;  // Bỏ qua nếu không tìm thấy target
                    }
                    
                    event_format::event evt;
                    
                    // Crop và encode thumbnail cho track này từ frame gốc
                    std::string thumbnail_image = "";
                    try {
                        // Crop ảnh từ frame gốc (không có bounding box) và resize về 150x150
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
                            thumbnail_image = base64_encode(buf.data(), buf.size(), false);
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
                    
                    track.class_label = "Face";
                    track.external_id = "a42f6aa6-637b-419f-a2dd-f036454a8cd5";
                    track.id = "FaceTracker_" + std::to_string(target_for_track->track_id);
                    track.last_seen = 0;
                    track.source_tracker_track_id = target_for_track->track_id;
                    
                    // Fill best_thumbnail cho event này
                    evt.best_thumbnail_obj.confidence = target_for_track->score;
                    evt.best_thumbnail_obj.image = thumbnail_image;
                    evt.best_thumbnail_obj.instance_id = instance_id_;
                    evt.best_thumbnail_obj.label = "Entered area";
                    evt.best_thumbnail_obj.system_date = get_current_date_iso();
                    evt.best_thumbnail_obj.tracks.push_back(track);
                    
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
            
            // Serialize to JSON - tạo mảng chứa một object [{...}]
            std::stringstream msg_stream;
            {
                cereal::JSONOutputArchive json_archive(msg_stream);
                std::vector<event_format::event_message> result_array;
                result_array.push_back(event_msg);
                json_archive(result_array);
            }
            
            // Bỏ wrapper "value0" từ JSON string nếu có
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
            broking_cache_ignore_threshold, encode_full_frame),
          mqtt_publisher_(mqtt_publisher),
          instance_id_(instance_id),
          zone_id_(zone_id),
          zone_name_(zone_name) {
    }
    
    ~cvedix_json_enhanced_mqtt_broker_node() = default;
};

int main(int argc, char** argv) {
    // Xử lý Ctrl+C
    std::signal(SIGINT, signal_handler);

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Tham số mặc định
    std::string model_path = "./cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn";
    std::string rtsp_url = "rtsp://103.147.186.175:18554/livestream/5F0459EPAG1EB5A";
    float score_threshold = 0.7f;
    float nms_threshold = 0.5f;
    int top_k = 50;
    int input_size = 320;  // width = height
    int min_face_size = 40;
    std::string mqtt_broker = "14.186.188.195";
    int mqtt_port = 30883;
    std::string mqtt_topic = "events";
    std::string mqtt_username = "";
    std::string mqtt_password = "";

    // Parse command line arguments
    if (argc > 1) {
        model_path = argv[1];
    }
    if (argc > 2) {
        rtsp_url = argv[2];
    }
    if (argc > 3) {
        score_threshold = std::stof(argv[3]);
    }
    if (argc > 4) {
        nms_threshold = std::stof(argv[4]);
    }
    if (argc > 5) {
        top_k = std::stoi(argv[5]);
    }
    if (argc > 6) {
        input_size = std::stoi(argv[6]);
    }
    if (argc > 7) {
        min_face_size = std::stoi(argv[7]);
    }
    if (argc > 8) {
        mqtt_broker = argv[8];
    }
    if (argc > 9) {
        mqtt_port = std::stoi(argv[9]);
    }
    if (argc > 10) {
        mqtt_topic = argv[10];
    }

    std::cout << "[Main] =========================================" << std::endl;
    std::cout << "[Main] RKNN Face Tracking Sample with MQTT" << std::endl;
    std::cout << "[Main] =========================================" << std::endl;
    std::cout << "[Main] Model: " << model_path << std::endl;
    std::cout << "[Main] RTSP URL: " << rtsp_url << std::endl;
    std::cout << "[Main] Score threshold: " << score_threshold << std::endl;
    std::cout << "[Main] NMS threshold: " << nms_threshold << std::endl;
    std::cout << "[Main] Top K: " << top_k << std::endl;
    std::cout << "[Main] Input size: " << input_size << "x" << input_size << std::endl;
    std::cout << "[Main] Min face size: " << min_face_size << " pixels" << std::endl;
    std::cout << "[Main] MQTT Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
    std::cout << "[Main] MQTT Topic: " << mqtt_topic << std::endl;
    std::cout << "[Main] =========================================" << std::endl;

    // Đầu vào: Nguồn RTSP
    auto rtsp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src_0", 0, rtsp_url, 0.8, "mppvideodec", 0, "auto");
    
    // Suy luận: Phát hiện khuôn mặt sử dụng RKNN NPU
    auto rknn_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_face_detector_node>(
        "rknn_face_detector_0", 
        model_path,
        score_threshold,
        nms_threshold,
        top_k,
        input_size,
        input_size
    );

    // Custom Data Transform Node: Filter theo kích thước bounding box
    auto custom_transform_0 = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
        "custom_transform_0",
        [min_face_size](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) -> std::shared_ptr<cvedix_objects::cvedix_frame_meta> {
            if (!meta || meta->face_targets.empty()) {
                return meta;
            }
            
            auto it = meta->face_targets.begin();
            while (it != meta->face_targets.end()) {
                const auto& face_target = *it;
                int min_dimension = std::min(face_target->width, face_target->height);
                int area = face_target->width * face_target->height;
                float aspect_ratio = (face_target->height > 0) ? 
                    static_cast<float>(face_target->width) / static_cast<float>(face_target->height) : 0.0f;
                
                bool is_valid = (min_dimension >= min_face_size) &&
                               (area >= min_face_size * min_face_size) &&
                               (aspect_ratio >= 0.5f && aspect_ratio <= 2.0f);
                
                if (!is_valid) {
                    it = meta->face_targets.erase(it);
                } else {
                    ++it;
                }
            }
            
            return meta;
        }
    );
    
    // Theo dõi: Bộ theo dõi SORT cho khuôn mặt
    auto sort_tracker_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_tracker_0", 
        cvedix_nodes::cvedix_track_for::FACE
    );
    
    // MQTT configuration
    std::cout << "[Main] Initializing MQTT publisher..." << std::endl;
    auto mqtt_publisher = std::make_unique<cvedix_utils::cvedix_mqtt_client>(
        mqtt_broker,
        mqtt_port,
        "rknn_face_tracking_publisher_" + std::to_string(std::time(nullptr)),
        60
    );
    
    mqtt_publisher->set_auto_reconnect(true, 5000);
    
    if (!mqtt_publisher->connect(mqtt_username, mqtt_password)) {
        std::cerr << "[Main] Warning: MQTT publisher connection failed: " << mqtt_publisher->get_last_error() << std::endl;
        std::cerr << "[Main] Continuing without MQTT publisher..." << std::endl;
    } else {
        int retry_count = 0;
        while (!mqtt_publisher->is_connected() && retry_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            retry_count++;
        }
        if (mqtt_publisher->is_connected()) {
            std::cout << "[Main] MQTT publisher connected successfully!" << std::endl;
        }
    }
    
    // MQTT publish function
    auto mqtt_publish_func = [&mqtt_publisher, &mqtt_topic](const std::string& message) {
        if (mqtt_publisher && mqtt_publisher->is_connected() && !message.empty()) {
            mqtt_publisher->publish(mqtt_topic, message);
        }
    };
    
    // Enhanced MQTT Broker: Crop ảnh từ frame gốc và gửi qua MQTT
    auto enhanced_mqtt_broker_0 = std::make_shared<cvedix_json_enhanced_mqtt_broker_node>(
        "enhanced_mqtt_broker_0",
        cvedix_nodes::cvedix_broke_for::FACE,
        100,  // broking_cache_warn_threshold
        500,  // broking_cache_ignore_threshold
        false, // encode_full_frame
        mqtt_publish_func,
        "DEMO",  // instance_id
        "95493308-c879-4f85-9fb7-36433971f60c",  // zone_id
        "Quan Giao"  // zone_name
    );
    
    // OSD: Vẽ kết quả lên khung hình
    auto face_osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("face_osd_0");

    // Split node: Chia output thành nhiều nhánh
    auto split_0 = std::make_shared<cvedix_nodes::cvedix_split_node>("split_0", false, false);
    
    // Screen display node
    std::shared_ptr<cvedix_nodes::cvedix_des_node> screen_des_0;
    const char* display = std::getenv("DISPLAY");
    bool use_screen = false;
    
    if (display != nullptr && std::strlen(display) > 0) {
        use_screen = false;
        CVEDIX_INFO("DISPLAY=" + std::string(display) + " detected, but using fake_des_node for headless server compatibility");
    } else {
        CVEDIX_INFO("No DISPLAY detected, using fake_des_node");
    }
    
    if (use_screen) {
        screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    } else {
        screen_des_0 = std::make_shared<cvedix_nodes::cvedix_fake_des_node>("screen_des_0", 0);
    }
    
    // RTMP output
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
        "rtmp_des_0", 
        0, 
        "rtmp://anhoidong.datacenter.cvedix.com:1935/live/rknn_face_tracking",
        cvedix_objects::cvedix_size{1280, 720},
        2048
    );

    // Xây dựng pipeline
    rknn_face_detector_0->attach_to({rtsp_src_0});
    custom_transform_0->attach_to({rknn_face_detector_0});
    sort_tracker_0->attach_to({custom_transform_0});
    
#ifdef CVEDIX_WITH_MQTT
    // Enhanced MQTT broker nhận từ tracker (trước OSD)
    enhanced_mqtt_broker_0->attach_to({sort_tracker_0});
    face_osd_0->attach_to({enhanced_mqtt_broker_0});
#else
    // OSD nhận trực tiếp từ tracker (khi không có MQTT)
    face_osd_0->attach_to({sort_tracker_0});
#endif
    
    split_0->attach_to({face_osd_0});
    screen_des_0->attach_to({split_0});
    rtmp_des_0->attach_to({split_0});

    // Khởi động pipeline
    std::cout << "[Main] Starting pipeline..." << std::endl;
    rtsp_src_0->start();

    // Bảng phân tích
    cvedix_utils::cvedix_analysis_board board({rtsp_src_0});
    board.display(1, false);

    std::cout << "[Main] Pipeline started. Press Ctrl+C to stop." << std::endl;
    
    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[Main] Shutting down..." << std::endl;
    rtsp_src_0->detach_recursively();
    
    std::cout << "[Main] Shutdown complete." << std::endl;
    
    return 0;
}
