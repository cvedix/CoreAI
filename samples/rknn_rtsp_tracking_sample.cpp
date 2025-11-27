#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/broker/cvedix_json_enhanced_console_broker_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <csignal>

#include <thread>
#include <chrono>

/*
* ## Ví dụ theo dõi đối tượng qua RTSP dùng RKNN ##
* Đầu vào RTSP → Bộ phát hiện RKNN YOLOv8 → Bộ theo dõi SORT → Broker (JSON Console) → OSD → Split → Hiển thị màn hình/RTMP
*
* URL: rtsp://admin:Admin123456@192.168.1.114:554/cam/realmonitor?channel=1&subtype=0
*
* Tính năng:
* - Tự động nhận biết codec (H264/H265) từ RTSP stream khi sử dụng codec_type="auto"
* - Node sẽ tự động phát hiện và cấu hình decoder phù hợp
* - Broker sẽ in JSON của frame_meta với base64 images ra console để debug:
*   + Base64 encoded full frame
*   + Base64 encoded crop images cho mỗi target
*   + Tọa độ bounding box (x, y, width, height, x1, y1, x2, y2, center_x, center_y)
*   + Độ chính xác (primary_score)
*   + Tên đối tượng (primary_label)
*   + Track ID và các thông tin liên quan
* - Broker forward dữ liệu đến OSD để đảm bảo pipeline hợp lệ (tất cả leaf nodes phải là DES nodes)
* - Pipeline vẫn chạy bình thường vì broker xử lý bất đồng bộ
*
* Yêu cầu:
* - Model RKNN (.rknn)
* - librknnrt.so (bắt buộc), librga.so (tùy chọn)
* - GStreamer (cho nguồn RTSP)
* - gst-discoverer-1.0 (cho tính năng auto-detection codec)
*
* Biên dịch:
*   cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_WITH_RTSP_SERVER=ON ..
*/

// Cờ toàn cục để xử lý tín hiệu
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

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

    // Đầu vào: Nguồn RTSP
    // codec_type="auto": Tự động nhận biết H264/H265 từ stream
    // Node sẽ sử dụng gst-discoverer-1.0 để phát hiện codec và cấu hình decoder phù hợp
    auto rtsp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src_0", 0, rtsp_url, 0.6, "mppvideodec", 0, "auto");
    
    // Suy luận: Phát hiện vật thể sử dụng RKNN YOLOv8
    auto rknn_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
        "rknn_detector_0", 
        model_path,
        0.25,  // ngưỡng điểm
        0.35, // ngưỡng NMS
        320,  // chiều rộng đầu vào
        320,  // chiều cao đầu vào
        1    // số lớp (COCO)
    );

    // Theo dõi: Bộ theo dõi SORT
    // Theo dõi vật thể chung (cvedix_track_for::NORMAL)
    auto sort_tracker_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_tracker_0", 
        cvedix_nodes::cvedix_track_for::NORMAL
    );
    
    // Broker: Gửi kết quả tracking ra console dưới dạng JSON với base64 images (debug)
    // Broker sẽ in JSON của frame_meta bao gồm:
    // - Base64 encoded full frame
    // - Base64 encoded crop images cho mỗi target
    // - Tọa độ bounding box (x, y, width, height, x1, y1, x2, y2, center_x, center_y)
    // - Độ chính xác (primary_score)
    // - Tên đối tượng (primary_label)
    // - Track ID và các thông tin liên quan
    // Broker forward dữ liệu đến OSD để đảm bảo pipeline hợp lệ (tất cả leaf nodes phải là DES nodes)
    // Pipeline vẫn chạy bình thường vì broker xử lý bất đồng bộ
    auto json_broker_0 = std::make_shared<cvedix_nodes::cvedix_json_enhanced_console_broker_node>(
        "json_broker_0", 
        cvedix_nodes::cvedix_broke_for::NORMAL  // Broker cho đối tượng thường (targets)
    );
    
    // OSD: Vẽ kết quả lên khung hình
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");

    // Split node: Chia luồng dữ liệu
    auto split_node_0 = std::make_shared<cvedix_nodes::cvedix_split_node>("split_node_0");
    
    // Đầu ra: Hiển thị lên màn hình
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Đầu ra: Gửi lên RTMP server
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://anhoidong.datacenter.cvedix.com:1935/live/2000", cvedix_objects::cvedix_size{1280, 720}, 1024 * 2);
    

    // Xây dựng pipeline
    rknn_detector_0->attach_to({rtsp_src_0});
    sort_tracker_0->attach_to({rknn_detector_0});
    json_broker_0->attach_to({sort_tracker_0});  // Broker nhận dữ liệu từ tracker
    osd_0->attach_to({json_broker_0});           // OSD nhận từ broker (broker forward dữ liệu)
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
    
    return 0;
}

