#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtsp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## multi_detectors_sample ##
* detect obstacles AND vehicles using yolo on road.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/unclear.mp4", 0.5);
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/test_video/roadblock.mp4", 0.6);
    auto obstacle_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("obstacle_detector", "./cvedix_data/models/det_cls/obstacles_yolov5s.onnx", "", "./cvedix_data/models/det_cls/obstacles_2classes.txt", 640, 640);
    // MUST set class_id_offset for the 2nd detector which is equal with total classes of the 1st detectors
    auto vehicle_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("vehicle_detector", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg", "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt", 416, 416, 1, 2);
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split_by_channel", true);
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);    
    auto srceen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);

    // construct pipeline
    obstacle_detector->attach_to({file_src_0, file_src_1});
    vehicle_detector->attach_to({obstacle_detector});
    osd->attach_to({vehicle_detector});
    split->attach_to({osd});
    screen_des_0->attach_to({split});
    srceen_des_1->attach_to({split});

    file_src_0->start();
    file_src_1->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({file_src_0, file_src_1});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
    file_src_1->detach_recursively();
}