#include "cvedix/nodes/src/cvedix_image_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_scanner.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## vehicle_body_scan_sample ##
* detect wheels and vehicle type based on side view of vehicle
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto image_src_0 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_src_0", 0, "./cvedix_data/test_images/body/%d.jpg");
    auto vehicle_scanner = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_scanner>("vehicle_scanner", "./cvedix_data/models/trt/vehicle/vehicle_scan_v8.5.trt");
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    vehicle_scanner->attach_to({image_src_0});
    osd->attach_to({vehicle_scanner});
    screen_des_0->attach_to({osd});

    image_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({image_src_0});
    board.display();
}