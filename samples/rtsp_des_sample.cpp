#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtsp_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## rtsp_des_sample ##
* show how cvedix_rtsp_des_node works, push video stream via rtsp, no specialized rtsp server needed.
* visit `rtsp://server-ip:rtsp_port/rtsp_name directly.
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/vehicle_count.mp4", 0.5);
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/test_video/vehicle_stop.mp4", 0.5);
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("yolo_detector", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights", "./cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg", "./cvedix_data/models/det_cls/yolov3_tiny_5classes.txt");
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split_by_channel", true);
    //auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto rtsp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtsp_des_node>("rtsp_des_0", 0, 8000, "rtsp_0");
    
    //auto srceen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);
    auto rtsp_des_1 = std::make_shared<cvedix_nodes::cvedix_rtsp_des_node>("rtsp_des_1", 1, 8000, "rtsp_1");

    // construct pipeline
    yolo_detector->attach_to({file_src_0, file_src_1});
    osd->attach_to({yolo_detector});
    split->attach_to({osd});
    rtsp_des_0->attach_to({split});
    rtsp_des_1->attach_to({split});

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