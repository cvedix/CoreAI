#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov8_detector.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov8_classifier.h"
#include "cvedix/nodes/osd/cvedix_osd_node_v3.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## trt yolov8 sample2 ##
* detection/classification using yolov8 based on tensorrt
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/mask_rcnn.mp4");
    auto yolo8_detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov8_detector>("yolo8_detector", "./cvedix_data/models/trt/others/yolov8s_v8.5.engine", "./cvedix_data/models/coco_80classes.txt");
    auto yolo8_classifier = std::make_shared<cvedix_nodes::cvedix_trt_yolov8_classifier>("yolo8_classifier", "./cvedix_data/models/trt/others/yolov8s-cls_v8.5.engine", "./cvedix_data/models/imagenet_1000labels1.txt");
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node_v3>("osd", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    yolo8_detector->attach_to({file_src_0});
    yolo8_classifier->attach_to({yolo8_detector});
    osd->attach_to({yolo8_classifier});
    screen_des_0->attach_to({osd});

    // start pipeline
    file_src_0->start();

    // visualize pipeline for debug
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}