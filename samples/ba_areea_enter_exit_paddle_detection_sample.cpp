/**
 * @file ba_areea_enter_exit_paddle_detection_sample.cpp
 * @brief Sample for area enter/exit BA node using PaddleDetection detector
 */

#include "cvedix/nodes/ba/cvedix_ba_area_enter_exit_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/infers/cvedix_paddle_detector_node.h"
#include "cvedix/nodes/infers/cvedix_paddle_attribute_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"

#include <iostream>
#include <string>

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG({"ba_area"});
    CVEDIX_LOGGER_INIT();

    CVEDIX_INFO("===== BA Area Enter/Exit + PaddleDetection Sample =====");

    // Create source node
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/test_video/vehicle_count.mp4", 0.6,
        true, "avdec_h264", 1);

    // Create PaddleDetection detector node (PP-YOLOE MOT vehicle model)
    auto detector = std::make_shared<cvedix_nodes::cvedix_paddle_detector_node>(
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
    // detector->set_allowed_classes({0, 1, 2, 3, 4, 5, 6});

    // Create tracker
    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "tracker", cvedix_nodes::cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

    // Create vehicle attribute node (secondary infer on vehicle ROIs)
    // labels_path + attribute_group_sizes should match your PP-LCNet model.
    auto attr = std::make_shared<cvedix_nodes::cvedix_paddle_attribute_node>(
        "vehicle_attr",
        "./cvedix_data/models/paddle/PP-LCNet_x1_0_vehicle_attribute_infer",
        "./cvedix_data/models/paddle/PP-LCNet_x1_0_vehicle_attribute_infer/labels.txt",
        true,
        0,
        "paddle",
        false,
        4,
        256,
        192,
        1,
        0.5f,
        std::vector<int>{},
        std::vector<int>{},
        0,
        0,
        10,
        false);

    // Define 2 polygonal areas for channel 0
    std::vector<cvedix_objects::cvedix_point> area0 = {
        {50, 150}, {250, 100}, {250, 350}, {50, 350}};
    std::vector<cvedix_objects::cvedix_point> area1 = {
        {350, 160}, {520, 100}, {550, 360}, {350, 360}};

    std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas = {
        {0, {area0, area1}}};

    cvedix_nodes::area_alert_config cfg0(true, true, "Entrance", cv::Scalar(0, 220, 0));
    cvedix_nodes::area_alert_config cfg1(true, true, "Restricted", cv::Scalar(0, 0, 220));
    std::map<int, std::vector<cvedix_nodes::area_alert_config>> configs = {
        {0, {cfg0, cfg1}}};

    // Create BA node with areas and per-area configs
    auto ba_area = std::make_shared<cvedix_nodes::cvedix_ba_area_enter_exit_node>(
        "ba_area", areas, configs, false, false);

    // Create OSD and output node
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd->update_config(osd_cfg);

    std::vector<cvedix_nodes::unified_static_zone_config> static_zones;
    static_zones.emplace_back(area0, cfg0.color, cfg0.name);
    static_zones.emplace_back(area1, cfg1.color, cfg1.name);
    osd->set_static_zones(static_zones);

    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
        "rtmp_des_0", 0, "rtmp://127.0.0.1/live/9000");

    // Build pipeline
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    attr->attach_to({tracker});
    ba_area->attach_to({attr});
    osd->attach_to({ba_area});
    rtmp_des_0->attach_to({osd});

    CVEDIX_INFO("Pipeline built: src -> paddle_detector -> tracker -> ba_area -> osd -> rtmp");
    CVEDIX_INFO("Starting pipeline... Press Enter to stop");

    file_src->start();

    std::string wait;
    std::getline(std::cin, wait);

    file_src->detach_recursively();
    return 0;
}
