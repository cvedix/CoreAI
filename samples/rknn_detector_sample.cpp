#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>

/*
* ## RKNN Detector Sample ##
* 1 video input → 1 RKNN primary detector (YOLOv8) → 1 output (OSD on screen)
* Uses generic YOLOv8 model (80 classes from COCO)
*
* Requirements:
* - RKNN Model (.rknn)
* - librknnrt.so (required), librga.so (optional)
*
* Build:
*   cmake -DCVEDIX_WITH_RKNN=ON [-DCVEDIX_WITH_RGA=ON] ..
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // Default model path
    std::string model_path = "./cvedix_data/models/yolov8n.rknn";
    if (argc > 1) {
        model_path = argv[1];
    }
    
    std::string video_path = "./cvedix_data/test_video/face_person.mp4";
    if (argc > 2) {
        video_path = argv[2];
    }

    CVEDIX_INFO("Using model: " + model_path);
    CVEDIX_INFO("Using video: " + video_path);

    // Input: Read video from file
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 0, video_path, 0.0); // 0.0 = infinite loop
    
    // Inference: Detect objects using RKNN YOLOv8
    // Default COCO 80 classes
    auto rknn_detector_0 = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
        "rknn_detector_0", 
        model_path,
        0.5,  // score threshold
        0.45, // NMS threshold
        640,  // input width
        640,  // input height
        80    // num classes (COCO)
    );
    
    // OSD: Draw results on frame
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    
    // Output: Display on screen
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Build pipeline
    rknn_detector_0->attach_to({file_src_0});
    osd_0->attach_to({rknn_detector_0});
    screen_des_0->attach_to({osd_0});

    // Start pipeline
    file_src_0->start();

    // Analysis board (GUI)
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display(1, false);

    // Wait for user input to stop
    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    
    return 0;
}

