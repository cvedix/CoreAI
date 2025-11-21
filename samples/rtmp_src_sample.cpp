#include "cvedix/nodes/src/cvedix_rtmp_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_detector.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
* ## rtmp_src_sample ##
* 1 rtmp video input, 1 infer task, and 1 output.
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto rtmp_src_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_src_node>("rtmp_src_0", 0, "rtmp://192.168.77.196/live/1000", 0.6);
    auto trt_vehicle_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_detector>("vehicle_detector", "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt");
    auto track_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("track_0");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://192.168.77.196/live/2000", cvedix_objects::cvedix_size{1280, 720}, 1024 * 2);

    // construct pipeline
    trt_vehicle_detector->attach_to({rtmp_src_0});
    track_0->attach_to({trt_vehicle_detector});
    osd_0->attach_to({track_0});
    rtmp_des_0->attach_to({osd_0});

    rtmp_src_0->start();

    // for debug purpose
    cvedix_utils::cvedix_analysis_board board({rtmp_src_0});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    rtmp_src_0->detach_recursively();
}