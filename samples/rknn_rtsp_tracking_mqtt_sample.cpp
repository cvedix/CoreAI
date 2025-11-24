#ifdef CVEDIX_WITH_RKNN

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

#ifdef CVEDIX_WITH_MQTT
// Custom Enhanced MQTT Broker Node: Kế thừa từ enhanced broker và gửi qua MQTT
class cvedix_json_enhanced_mqtt_broker_node : public cvedix_nodes::cvedix_json_enhanced_console_broker_node {
private:
    std::function<void(const std::string&)> mqtt_publisher_;

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
        std::function<void(const std::string&)> mqtt_publisher)
        : cvedix_nodes::cvedix_json_enhanced_console_broker_node(
            node_name, broke_for, broking_cache_warn_threshold, 
            broking_cache_ignore_threshold, encode_full_frame)
        , mqtt_publisher_(mqtt_publisher)
    {
    }
    
    ~cvedix_json_enhanced_mqtt_broker_node() = default;
    
    void set_mqtt_publisher(std::function<void(const std::string&)> publisher) {
        mqtt_publisher_ = publisher;
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
    std::string model_path = "./cvedix_data/models/yolov8n.rknn";
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
    auto rtsp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src_0", 0, rtsp_url, 0.6, "mppvideodec", 0, "auto");
    
    // Suy luận: Phát hiện vật thể sử dụng RKNN YOLOv8
    auto rknn_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
        "rknn_detector_0", 
        model_path,
        0.25,  // ngưỡng điểm
        0.35, // ngưỡng NMS
        640,  // chiều rộng đầu vào
        640,  // chiều cao đầu vào
        80    // số lớp (COCO)
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
            auto it = meta->targets.begin();
            while (it != meta->targets.end()) {
                if ((*it)->primary_score < 0.3f) {
                    it = meta->targets.erase(it);
                } else {
                    ++it;
                }
            }
            
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

#else
#include <iostream>
int main() {
    std::cerr << "RKNN support not enabled. Build with -DCVEDIX_WITH_RKNN=ON" << std::endl;
    return 1;
}
#endif // CVEDIX_WITH_RKNN

