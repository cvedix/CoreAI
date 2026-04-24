/**
 * @file ba_crossline_paddle_detection_sample.cpp
 * @brief BA crossline sample using PaddleDetection detector with full OSD features enabled
 */

#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/infers/cvedix_paddle_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"

#include <iostream>
#include <string>

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_crossline"});
    CVEDIX_LOGGER_INIT();

    CVEDIX_INFO("===== BA Crossline + PaddleDetection Sample =====");

    // Create source node
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 0, "./cvedix_data/test_video/vehicle_count.mp4", 0.4);

    // Create PaddleDetection detector node (PP-YOLOE MOT vehicle model)
    auto paddle_detector = std::make_shared<cvedix_nodes::cvedix_paddle_detector_node>(
        "paddle_detector",
        "./cvedix_data/models/paddle/mot_ppyoloe_s_36e_ppvehicle",
        "",
        true,
        0,
        "paddle",
        false,
        4,
        1,
        0.45f,
        0,
        1,
        1280,
        640,
        false);

    // Keep only vehicle classes if needed (optional)
    // paddle_detector->set_allowed_classes({0, 1, 2, 3, 4, 5, 6});

    // Create tracker
    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "track_0", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

    // Define one crossline for channel 0
    cvedix_objects::cvedix_point start(0, 250);
    cvedix_objects::cvedix_point end(700, 220);
    std::map<int, cvedix_objects::cvedix_line> lines = {
        {0, cvedix_objects::cvedix_line(start, end)}};

    auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>(
        "ba_crossline", lines);

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

    // Enable full OSD features
    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd_cfg.show_track_id = true;
    osd_cfg.show_track_trail = true;
    osd_cfg.show_center_dot = true;
    osd_cfg.show_sub_targets = true;

    osd_cfg.enable_ba_crossline = true;
    osd_cfg.enable_ba_crowding = true;
    osd_cfg.enable_ba_jam = true;
    osd_cfg.enable_ba_stop = true;
    osd_cfg.enable_ba_enter_exit = true;

    osd_cfg.enable_face = true;
    osd_cfg.enable_pose = true;
    osd_cfg.enable_instance_mask = true;
    osd_cfg.enable_text_region = true;
    osd_cfg.enable_expr = true;
    osd_cfg.enable_lane = true;
    osd_cfg.enable_plate = true;
    osd_cfg.enable_seg = true;
    osd_cfg.enable_mllm = true;
    osd_cfg.enable_sub_thumbnails = true;

    osd_cfg.show_static_lines = true;
    osd_cfg.show_static_zones = true;
    osd->update_config(osd_cfg);

    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
        "rtmp_des_0", 0, "rtmp://127.0.0.1/live/9000");

    // Construct pipeline
    paddle_detector->attach_to({file_src_0});
    tracker->attach_to({paddle_detector});
    ba_crossline->attach_to({tracker});
    osd->attach_to({ba_crossline});
    rtmp_des_0->attach_to({osd});

    CVEDIX_INFO("Pipeline built: src -> paddle_detector -> tracker -> ba_crossline -> osd -> rtmp");
    CVEDIX_INFO("Starting pipeline... Press Enter to stop");

    file_src_0->start();

    std::string wait;
    std::getline(std::cin, wait);

    file_src_0->detach_recursively();
    return 0;
}
