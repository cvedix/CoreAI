#include "cvedix/nodes/src/cvedix_image_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_plate_detector_v2.h"
#include "cvedix/nodes/osd/cvedix_plate_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## plate recognize sample ##
* detect and recognize plate in the whole frame
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto image_src_0 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_src_0", 0, "./cvedix_data/test_images/plates/%d.jpg", 1);
    auto plate_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_plate_detector_v2>("plate_detector", "./cvedix_data/models/trt/plate/det_v8.5.trt", "./cvedix_data/models/trt/plate/rec_v8.5.trt");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_plate_osd_node>("osd_0", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline 
    plate_detector->attach_to({image_src_0});
    osd_0->attach_to({plate_detector});
    screen_des_0->attach_to({osd_0});

    // start pipeline
    image_src_0->start();

    // visualize pipeline for debug
    cvedix_utils::cvedix_analysis_board board({image_src_0});
    board.display();
}