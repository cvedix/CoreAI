#include "cvedix/nodes/src/cvedix_image_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_scanner.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_plate_detector_v2.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/osd/cvedix_plate_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## body_scan_and_plate_detect_sample ##
* first channel detects wheels and vehicle type based on side view of vehicle
* second channel detects plate based on head view of vehicle
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto image_src_0 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_src_0", 0, "./cvedix_data/test_images/body/%d.jpg");
    auto image_src_1 = std::make_shared<cvedix_nodes::cvedix_image_src_node>("image_src_1", 1, "./cvedix_data/test_images/plates/%d.jpg");
    auto vehicle_scanner = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_scanner>("vehicle_scanner", 
                                                                            "./cvedix_data/models/trt/vehicle/vehicle_scan_v8.5.trt");
    auto plate_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_plate_detector_v2>("plate_detector", 
                                                                                    "./cvedix_data/models/trt/plate/det_v8.5.trt", 
                                                                                    "./cvedix_data/models/trt/plate/rec_v8.5.trt");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    auto osd_1 = std::make_shared<cvedix_nodes::cvedix_plate_osd_node>("osd_1", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto screen_des_1 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_1", 1);

    // construct first pipeline
    vehicle_scanner->attach_to({image_src_0});
    osd_0->attach_to({vehicle_scanner});
    screen_des_0->attach_to({osd_0});
    // construct second pipeline
    plate_detector->attach_to({image_src_1});
    osd_1->attach_to({plate_detector});
    screen_des_1->attach_to({osd_1});

    image_src_0->start();
    image_src_1->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({image_src_0, image_src_1});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    image_src_0->detach_recursively();
    image_src_1->detach_recursively();
}