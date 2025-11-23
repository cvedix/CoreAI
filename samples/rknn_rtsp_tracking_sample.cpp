#ifdef CVEDIX_WITH_RKNN

#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <csignal>

#include <thread>
#include <chrono>

/*
* ## Ví dụ theo dõi đối tượng qua RTSP dùng RKNN ##
* Đầu vào RTSP → Bộ phát hiện RKNN YOLOv8 → Bộ theo dõi SORT → OSD → Hiển thị màn hình
*
* URL: rtsp://admin:Admin123456@192.168.1.114:554/cam/realmonitor?channel=1&subtype=0
*
* Tính năng:
* - Tự động nhận biết codec (H264/H265) từ RTSP stream khi sử dụng codec_type="auto"
* - Node sẽ tự động phát hiện và cấu hình decoder phù hợp
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
    std::string model_path = "./cvedix_data/models/yolov8n.rknn";
    if (argc > 1) {
        model_path = argv[1];
    }
    
    std::string rtsp_url = "rtsp://103.147.186.175:18554/9L02DA3PAJ39B2F";
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

    // Split node: Chia luồng dữ liệu
    auto split_node_0 = std::make_shared<cvedix_nodes::cvedix_split_node>("split_node_0");
    
    // Đầu ra: Hiển thị lên màn hình
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Đầu ra: Gửi lên RTMP server
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://anhoidong.datacenter.cvedix.com:1935/live/2000", cvedix_objects::cvedix_size{1280, 720}, 1024 * 2);
    

    // Xây dựng pipeline
    rknn_detector_0->attach_to({rtsp_src_0});
    sort_tracker_0->attach_to({rknn_detector_0});
    osd_0->attach_to({sort_tracker_0});
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

#else
#include <iostream>
int main() {
    std::cerr << "RKNN support not enabled. Build with -DCVEDIX_WITH_RKNN=ON" << std::endl;
    return 1;
}
#endif // CVEDIX_WITH_RKNN
