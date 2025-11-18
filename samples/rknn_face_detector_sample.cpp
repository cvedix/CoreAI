#ifdef CVEDIX_WITH_RKNN

#include "../nodes/cvedix_file_src_node.h"
#include "../nodes/infers/cvedix_yolo_rknn_face_detector_node.h"
#include "../nodes/osd/cvedix_face_osd_node_v2.h"
#include "../nodes/cvedix_screen_des_node.h"

#include "../utils/analysis_board/cvedix_analysis_board.h"

/*
* ## RKNN Face Detector Sample ##
* Face detection using Rockchip NPU (RKNN) with hardware acceleration
* Optimized for RK3566, RK3568, RK3588 chips
* Uses RGA for hardware-accelerated preprocessing when available
* 
* Requirements:
* - RKNN model file (.rknn format)
* - librknnrt.so installed
* - librga.so installed (optional, for preprocessing acceleration)
* 
* Build with:
*   cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_WITH_RGA=ON ..
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // Model path - update this to your RKNN model path
    std::string model_path = "./cvedix_data/models/face/yolov8n_face_detection.rknn";
    
    // If model path not provided as argument, use default
    // You can modify this to accept command line arguments
    
    // Create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 0, "./cvedix_data/test_video/face.mp4", 0.6);
    
    // Create RKNN face detector node
    // Parameters: node_name, model_path, score_threshold, nms_threshold, input_width, input_height
    auto rknn_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yolo_rknn_face_detector_node>(
        "rknn_face_detector_0", 
        model_path,
        0.5,  // score threshold
        0.5,  // NMS threshold
        640,  // input width
        640   // input height
    );
    
    // OSD node to draw detection results
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_0");
    
    // Screen output node
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Construct pipeline
    rknn_face_detector_0->attach_to({file_src_0});
    osd_0->attach_to({rknn_face_detector_0});
    screen_des_0->attach_to({osd_0});

    // Start pipeline
    file_src_0->start();

    // Visualization board for debugging
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    // Wait for user input to stop
    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    
    return 0;
}

#else
#include <iostream>
int main() {
    std::cerr << "RKNN support not enabled. Build with -DCVEDIX_WITH_RKNN=ON" << std::endl;
    return 1;
}
#endif // CVEDIX_WITH_RKNN

