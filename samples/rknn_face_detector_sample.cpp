#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>

/*
* ## RKNN Face Detector Sample ##
* 1 video input → 1 RKNN primary detector → 1 output (OSD on screen)
* Logic tương tự sample 1-1-1 nhưng sử dụng node RKNN YOLOv8.
*
* Môi trường yêu cầu:
* - Model RKNN (.rknn)
* - librknnrt.so (bắt buộc), librga.so (tùy chọn)
*
* Biên dịch:
*   cmake -DCVEDIX_WITH_RKNN=ON [-DCVEDIX_WITH_RGA=ON] ..
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // Model path - update this to your RKNN model path
    std::string model_path = "./cvedix_data/models/face/yolov8n_face_detection.rknn";
    
    // If model path not provided as argument, use default
    // You can modify this to accept command line arguments
    
    // Đầu vào: đọc video từ file (có thể thay bằng RTSP/RTMP...)
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 0, "./cvedix_data/test_video/face_multis.mp4", 0.6);
    
    // Suy luận cấp 1: phát hiện khuôn mặt bằng RKNN YOLOv8
    auto rknn_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
        "rknn_face_detector_0", 
        model_path,
        0.5,  // score threshold
        0.5,  // NMS threshold
        640,  // input width
        640,  // input height
        1     // num classes (face detector model)
    );

    // Theo dõi: Bộ theo dõi SORT
    // Theo dõi vật thể chung (cvedix_track_for::NORMAL)
    auto sort_tracker_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "sort_tracker_0", 
        cvedix_nodes::cvedix_track_for::NORMAL
    );
    
    // OSD: vẽ kết quả lên khung hình
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_0");
    
    // Split: Chia output OSD thành 2 nhánh (hiển thị và RTMP)
    auto split_0 = std::make_shared<cvedix_nodes::cvedix_split_node>("split_0", false, false);

    // Đầu ra: hiển thị lên màn hình (tự động chọn sink theo environment)
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://anhoidong.datacenter.cvedix.com:1935/live/camera_ai");

    // Xây pipeline
    rknn_face_detector_0->attach_to({file_src_0});
    sort_tracker_0->attach_to({rknn_face_detector_0});
    osd_0->attach_to({sort_tracker_0});
    split_0->attach_to({osd_0});
    
    // Branch 1: Screen
    screen_des_0->attach_to({split_0});
    
    // Branch 2: RTMP
    rtmp_des_0->attach_to({split_0});

    // Khởi động pipeline
    file_src_0->start();

    // Phân tích pipeline (chỉ bật nếu có GUI – tránh lỗi GTK khi chạy headless)
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    // Chờ người dùng dừng pipeline
    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    
    return 0;
}

