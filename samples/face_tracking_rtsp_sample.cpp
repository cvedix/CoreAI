#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/des/cvedix_fake_des_node.h"
#include <cstdlib>

#ifdef CVEDIX_WITH_MQTT
#include "cvedix/nodes/broker/cvedix_json_enhanced_console_broker_node.h"
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include "cvedix/nodes/broker/cereal_archive/cvedix_objects_cereal_archive.h"
#include "cpp_base64/base64.h"
#include <memory>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>
#include <mutex>
#include <opencv2/imgcodecs.hpp>
#endif
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_custom_data_transform_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <csignal>
#include <thread>
#include <chrono>

/*
* ## face tracking RTSP sample ##
* Track multiple faces from RTSP stream using cvedix_sort_track_node.
* 
* Pipeline:
* RTSP Source → YuNet Face Detector → SORT Tracker → [MQTT Broker] → Face OSD → Split → [Screen Display | RTMP | RTSP]
*
* Features:
* - Face detection using YuNet model
* - Multi-face tracking using SORT algorithm
* - Real-time display with tracking information
* - RTMP output stream: rtmp://anhoidong.datacenter.cvedix.com:1935/live/2001
* - RTSP output stream on port 2001 (rtsp://localhost:2001/2001) - optional
* - MQTT publisher on topic "2001" with cropped images (if CVEDIX_WITH_MQTT enabled)
*
* Build:
*   cmake -DCVEDIX_WITH_RTSP_SERVER=ON -DCVEDIX_WITH_MQTT=ON ..
*   make
*
* Usage:
*   ./face_tracking_rtsp_sample [rtsp_url] [score_threshold] [nms_threshold] [top_k] [min_face_size]
*
* Example:
*   ./face_tracking_rtsp_sample rtsp://admin:password@192.168.1.100:554/stream
*   # Mặc định: score=0.90, nms=0.35, top_k=20, min_face_size=40 (tối ưu cho độ chính xác ~90%)
*   ./face_tracking_rtsp_sample rtsp://... 0.90 0.35 20 40
*   ./face_tracking_rtsp_sample rtsp://... 0.3 0.4 100 20  # Nhận diện nhiều khuôn mặt hơn (độ chính xác thấp hơn)
*   ./face_tracking_rtsp_sample rtsp://... 0.95 0.3 15 50  # Độ chính xác rất cao, chỉ khuôn mặt lớn
*
* Output:
*   - Screen: Real-time display
*   - RTMP: rtmp://anhoidong.datacenter.cvedix.com:1935/live/2001
*   - RTSP: rtsp://localhost:2001/2001 (optional, if CVEDIX_WITH_RTSP_SERVER=ON)
*   - MQTT: Topic "2001" on broker anhoidong.datacenter.cvedix.com:1883 (with cropped images)
*/

// Global flag for signal handling
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

#ifdef CVEDIX_WITH_MQTT
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
#endif // CVEDIX_WITH_MQTT

int main(int argc, char** argv) {
    // Handle Ctrl+C
    std::signal(SIGINT, signal_handler);

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // RTSP URL - can be provided as command line argument
    std::string rtsp_url = "rtsp://103.147.186.175:18554/livestream/5F0459EPAG1EB5A";
    if (argc > 1) {
        rtsp_url = argv[1];
    }
    
    // Detection thresholds - tối ưu cho độ chính xác ~90%
    // Usage: ./face_tracking_rtsp_sample [rtsp_url] [score_threshold] [nms_threshold] [top_k] [min_face_size]
    // Để đạt độ chính xác cao (90%+), cần tăng score_threshold và filter theo kích thước bounding box
    float score_threshold = 0.90f;  // Ngưỡng confidence (0.0-1.0), tăng lên 0.90 để đạt độ chính xác ~90%
    float nms_threshold = 0.35f;     // Ngưỡng NMS (0.0-1.0), giảm xuống 0.35 để loại bỏ box trùng lặp tốt hơn
    int top_k = 20;                 // Số khuôn mặt tối đa, giảm xuống 20 để tập trung vào detections tốt nhất
    int min_face_size = 40;        // Kích thước tối thiểu của khuôn mặt (width hoặc height) để tránh false positives nhỏ
    
    if (argc > 2) {
        score_threshold = std::stof(argv[2]);
    }
    if (argc > 3) {
        nms_threshold = std::stof(argv[3]);
    }
    if (argc > 4) {
        top_k = std::stoi(argv[4]);
    }
    if (argc > 5) {
        min_face_size = std::stoi(argv[5]);
    }
    
    CVEDIX_INFO("Detection thresholds - score: " + std::to_string(score_threshold) + 
                ", nms: " + std::to_string(nms_threshold) + 
                ", top_k: " + std::to_string(top_k) +
                ", min_face_size: " + std::to_string(min_face_size));

    // Create nodes
    // RTSP source node
    auto rtsp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src_0", 0, rtsp_url, 0.8, "mppvideodec", 0, "auto");
    
    // Face detector using YuNet model - Tối ưu cho độ chính xác ~90%
    // Tham số threshold đã được điều chỉnh:
    // - score_threshold: 0.90 - Chỉ giữ detections có confidence rất cao (≥90%), giảm tối đa false positives
    // - nms_threshold: 0.35 - Loại bỏ box trùng lặp tốt hơn, giữ box tốt nhất
    // - top_k: 20 - Tập trung vào detections tốt nhất, giảm noise
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
        "yunet_face_detector_0", 
        "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
        score_threshold,
        nms_threshold,
        top_k);
    
    // Face feature encoder using SFace model
    // auto sface_face_encoder_0 = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>(
    //     "sface_face_encoder_0", 
    //     "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx");
    
    // Custom Data Transform Node: Filter theo kích thước bounding box để tránh false positives nhỏ
    // Loại bỏ các khuôn mặt quá nhỏ (thường là noise/false positives)
    auto custom_transform_0 = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
        "custom_transform_0",
        [min_face_size](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) -> std::shared_ptr<cvedix_objects::cvedix_frame_meta> {
            if (!meta || meta->face_targets.empty()) {
                return meta; // Forward as-is if no face targets
            }
            
            // Filter face_targets theo kích thước tối thiểu
            // Loại bỏ các khuôn mặt quá nhỏ (thường là false positives)
            auto it = meta->face_targets.begin();
            while (it != meta->face_targets.end()) {
                const auto& face_target = *it;
                
                // Tính kích thước tối thiểu (min của width và height)
                int min_dimension = std::min(face_target->width, face_target->height);
                
                // Tính diện tích bounding box
                int area = face_target->width * face_target->height;
                
                // Tính aspect ratio (tỷ lệ width/height)
                float aspect_ratio = (face_target->height > 0) ? 
                    static_cast<float>(face_target->width) / static_cast<float>(face_target->height) : 0.0f;
                
                // Filter conditions:
                // 1. Kích thước tối thiểu (width hoặc height) phải >= min_face_size
                // 2. Diện tích phải đủ lớn (ít nhất min_face_size^2)
                // 3. Aspect ratio hợp lý cho khuôn mặt (0.5 - 2.0, tức là không quá dẹt hoặc quá cao)
                bool is_valid = (min_dimension >= min_face_size) &&
                               (area >= min_face_size * min_face_size) &&
                               (aspect_ratio >= 0.5f && aspect_ratio <= 2.0f);
                
                if (!is_valid) {
                    // Loại bỏ face target không hợp lệ
                    it = meta->face_targets.erase(it);
                } else {
                    ++it;
                }
            }
            
            return meta;
        }
    );
    
    // SORT tracker for face tracking
    auto track_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "track_0", 
        cvedix_nodes::cvedix_track_for::FACE);   // track for face
    
    // Face OSD node for drawing tracking information
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd_0");
    
    // Split node: Chia output thành nhiều nhánh (Screen, RTSP, RTMP)
    auto split_0 = std::make_shared<cvedix_nodes::cvedix_split_node>("split_0", false, false);
    
    // Screen display node - chỉ dùng khi có DISPLAY hợp lệ, nếu không dùng fake_des_node
    // Luôn dùng fake_des_node trên headless server để tránh lỗi X display
    std::shared_ptr<cvedix_nodes::cvedix_des_node> screen_des_0;
    const char* display = std::getenv("DISPLAY");
    bool use_screen = false;
    
    // Kiểm tra DISPLAY và thử test xem có thể mở được không
    if (display != nullptr && std::strlen(display) > 0) {
        // Kiểm tra xem DISPLAY có hợp lệ không bằng cách test với xdpyinfo hoặc đơn giản là kiểm tra format
        // Nếu DISPLAY có format hợp lệ (như :0, :0.0, localhost:0, etc), có thể thử dùng screen
        // Nhưng để an toàn, trên server thường không có X, nên mặc định dùng fake_des_node
        // Chỉ dùng screen_des_node nếu chắc chắn có X server (ví dụ: desktop environment)
        use_screen = false;  // Mặc định không dùng screen để tránh lỗi
        CVEDIX_INFO("DISPLAY=" + std::string(display) + " detected, but using fake_des_node for headless server compatibility");
    } else {
        CVEDIX_INFO("No DISPLAY detected, using fake_des_node");
    }
    
    if (use_screen) {
        screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    } else {
        screen_des_0 = std::make_shared<cvedix_nodes::cvedix_fake_des_node>("screen_des_0", 0);
    }
    
    // RTMP output - stream name "2001"
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
        "rtmp_des_0", 
        0, 
        "rtmp://anhoidong.datacenter.cvedix.com:1935/live/2001",
        cvedix_objects::cvedix_size{1280, 720},  // resolution
        2048  // bitrate (2Mbps)
    );

#ifdef CVEDIX_WITH_MQTT
    // MQTT configuration
    std::string mqtt_broker = "14.186.188.195";
    int mqtt_port = 30883;
    std::string mqtt_topic = "events";  // Topic là "2001"
    std::string mqtt_username = "";
    std::string mqtt_password = "";
    
    // Initialize MQTT publisher
    std::cout << "[Main] Initializing MQTT publisher..." << std::endl;
    std::cout << "[Main] Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
    std::cout << "[Main] Topic: " << mqtt_topic << std::endl;
    
    auto mqtt_publisher = std::make_unique<cvedix_utils::cvedix_mqtt_client>(
        mqtt_broker,
        mqtt_port,
        "face_tracking_publisher_" + std::to_string(std::time(nullptr)),
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
    
    // Enhanced MQTT Broker: Crop ảnh từ frame gốc và gửi qua MQTT với format mới
    auto enhanced_mqtt_broker_0 = std::make_shared<cvedix_json_enhanced_mqtt_broker_node>(
        "enhanced_mqtt_broker_0",
        cvedix_nodes::cvedix_broke_for::FACE,
        100,  // broking_cache_warn_threshold
        500,  // broking_cache_ignore_threshold
        false, // encode_full_frame (tắt để tiết kiệm memory, chỉ encode crop images)
        mqtt_publish_func,
        "DEMO",  // instance_id
        "95493308-c879-4f85-9fb7-36433971f60c",  // zone_id
        "Quan Giao"  // zone_name
    );
#endif

    // Construct pipeline
    yunet_face_detector_0->attach_to({rtsp_src_0});
    // sface_face_encoder_0->attach_to({yunet_face_detector_0});
    
    // Custom Transform: Filter theo kích thước bounding box (trước tracker)
    // Loại bỏ các khuôn mặt quá nhỏ để tránh false positives
    custom_transform_0->attach_to({yunet_face_detector_0});
    track_0->attach_to({custom_transform_0});
    
#ifdef CVEDIX_WITH_MQTT
    // Enhanced MQTT broker nhận từ tracker (trước OSD) - crop ảnh từ frame gốc
    enhanced_mqtt_broker_0->attach_to({track_0});
    osd_0->attach_to({enhanced_mqtt_broker_0});
#else
    osd_0->attach_to({track_0});
#endif
    
    // Split output từ OSD
    split_0->attach_to({osd_0});
    
    // Branch 1: Screen display
    screen_des_0->attach_to({split_0});
    
    // Branch 2: RTMP stream
    rtmp_des_0->attach_to({split_0});


    // Start RTSP source
    rtsp_src_0->start();

    // For debug purpose - analysis board
    cvedix_utils::cvedix_analysis_board board({rtsp_src_0});
    
    // Run until interrupted
    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    rtsp_src_0->detach_recursively();

    return 0;
}

