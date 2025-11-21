#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_vehicle_detector.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/test_video/vehicle_stop.mp4", 0.5);
    auto trt_vehicle_detector = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_detector>("vehicle_detector", "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt");
    auto track_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("track_0");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0", "./cvedix_data/font/NotoSansCJKsc-Medium.otf");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // construct pipeline
    trt_vehicle_detector->attach_to({file_src_0});
    track_0->attach_to({trt_vehicle_detector});
    osd_0->attach_to({track_0});
    screen_des_0->attach_to({osd_0});

    // start pipeline
    file_src_0->start();

    // visualize pipeline for debug
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}