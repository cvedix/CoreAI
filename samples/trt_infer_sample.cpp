#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_detector.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_plate_detector.h"
#include "cvedix/nodes/osd/cvedix_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"

/*
* ## trt infer sample ##
* vehicle and plate detector based on tensorrt (install tensorrt first!)
* 1 video input and 3 outputs (screen, file, rtmp)
*/

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/plate.mp4");
    auto trt_vehicle_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_detector>("vehicle_detector", "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt");
    auto trt_vehicle_plate_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_plate_detector>("vehicle_plate_detector", "./cvedix_data/models/trt/plate/det_v8.5.trt", "./cvedix_data/models/trt/plate/rec_v8.5.trt");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node_v2>("osd_0", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0, true, cvedix_objects::cvedix_size{640, 360});
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://192.168.77.60/live/10000", cvedix_objects::cvedix_size{1280, 720});
    auto file_des_0 = std::make_shared<cvedix_nodes::cvedix_file_des_node>("file_des_0", 0, ".");

    // construct pipeline
    trt_vehicle_detector->attach_to({file_src_0});
    trt_vehicle_plate_detector->attach_to({trt_vehicle_detector});
    osd_0->attach_to({trt_vehicle_plate_detector});

    // split into 3 sub branches automatically
    screen_des_0->attach_to({osd_0});
    rtmp_des_0->attach_to({osd_0});
    file_des_0->attach_to({osd_0});

    // start pipeline
    file_src_0->start();

    // visualize pipeline for debug
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}