#ifdef CVEDIX_WITH_RKNN

#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <csignal>

/*
* ## Ví dụ theo dõi đối tượng qua RTSP dùng RKNN ##
* Đầu vào RTSP → Bộ phát hiện RKNN YOLOv8 → Bộ theo dõi SORT → OSD → Hiển thị màn hình
*
* URL: rtsp://admin:Admin123456@192.168.1.114:554/cam/realmonitor?channel=1&subtype=0
*
* Yêu cầu:
* - Model RKNN (.rknn)
* - librknnrt.so (bắt buộc), librga.so (tùy chọn)
* - GStreamer (cho nguồn RTSP)
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
    
    std::string rtsp_url = "rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo";
    if (argc > 2) {
        rtsp_url = argv[2];
    }

    CVEDIX_INFO("Sử dụng model: " + model_path);
    CVEDIX_INFO("Sử dụng RTSP URL: " + rtsp_url);

    // Đầu vào: Nguồn RTSP
    auto rtsp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src_0", 0, rtsp_url, 0.6);
    
    // Suy luận: Phát hiện vật thể sử dụng RKNN YOLOv8
    auto rknn_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
        "rknn_detector_0", 
        model_path,
        0.5,  // ngưỡng điểm
        0.45, // ngưỡng NMS
        640,  // chiều rộng đầu vào
        640,  // chiều cao đầu vào
        80    // số lớp (COCO)
    );

    // Theo dõi: Bộ theo dõi SORT
    // Theo dõi vật thể chung (cvedix_track_for::NORMAL)
    auto sort_tracker_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_tracker_0", 
        cvedix_nodes::cvedix_track_for::NORMAL
    );
    
    // OSD: Vẽ kết quả lên khung hình
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    
    // Đầu ra: Hiển thị lên màn hình
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Xây dựng pipeline
    rknn_detector_0->attach_to({rtsp_src_0});
    sort_tracker_0->attach_to({rknn_detector_0});
    osd_0->attach_to({sort_tracker_0});
    screen_des_0->attach_to({osd_0});

    // Khởi động pipeline
    rtsp_src_0->start();

    // Bảng phân tích (GUI)
    const bool has_display = (std::getenv("DISPLAY") && std::strlen(std::getenv("DISPLAY")) > 0) ||
                             (std::getenv("WAYLAND_DISPLAY") && std::strlen(std::getenv("WAYLAND_DISPLAY")) > 0);
    if (has_display) {
        cvedix_utils::cvedix_analysis_board board({rtsp_src_0});
        board.display(1, false);
    } else {
        CVEDIX_INFO("[rknn_rtsp_tracking_sample] Không tìm thấy DISPLAY, chạy ở chế độ không giao diện (headless). Nhấn Ctrl+C để dừng.");
        while (!stop_flag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // Dọn dẹp
    rtsp_src_0->detach_recursively();
    
    return 0;
}

#else
#include <iostream>
int main() {
    std::cerr << "RKNN support not enabled. Build with -DCVEDIX_WITH_RKNN=ON" << std::endl;
    return 1;
}
#endif // CVEDIX_WITH_RKNN
