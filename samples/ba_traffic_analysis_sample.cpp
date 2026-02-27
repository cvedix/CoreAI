/**
 * @file ba_traffic_analysis_sample.cpp
 * @brief Combined: Speed estimation + Accident detection on traffic video
 *
 * Pipeline:
 *   file_src → detector → tracker → speed_estimation → accident_detection → osd → file_des
 *
 * Requires: -DCVEDIX_WITH_TRT=ON
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_det_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_speed_estimation_node.h"
#include "cvedix/nodes/ba/cvedix_ba_accident_detection_node.h"
#include "cvedix/nodes/osd/cvedix_ba_line_crossline_osd_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"

#include <thread>

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::string video_path = "./cvedix_data/test_video/0206.mp4";
    std::string engine_path = "./cvedix_data/models/yolov11n.engine";
    int duration = 20;
    if (argc > 1) video_path = argv[1];
    if (argc > 2) engine_path = argv[2];
    if (argc > 3) duration = std::stoi(argv[3]);

    // === 1. Source ===
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 1.0, false
    );

    // === 2. Detector ===
    auto detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_det_node>(
        "detector", engine_path,
        "./cvedix_data/models/coco_80_labels_list.txt",
        0.15f, 0.45f
    );
    detector->set_allowed_classes({0, 1, 2, 3, 5, 7}); // person,bicycle,car,motorcycle,bus,truck

    // === 3. Tracker: ByteTrack ===
    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "tracker",
        cvedix_nodes::cvedix_track_for::NORMAL,
        0.1f, 0.5f, 0.7f, 90, 30
    );

    // === 4. Speed Estimation ===
    cvedix_objects::cvedix_line line1(
        cvedix_objects::cvedix_point(0, 400),
        cvedix_objects::cvedix_point(1920, 400)
    );
    cvedix_objects::cvedix_line line2(
        cvedix_objects::cvedix_point(0, 550),
        cvedix_objects::cvedix_point(1920, 550)
    );

    std::map<int, std::pair<cvedix_objects::cvedix_line, cvedix_objects::cvedix_line>> line_pairs = {
        {0, {line1, line2}}
    };
    std::map<int, double> pixel_to_meter = {
        {0, 15.0 / 150.0}
    };

    auto speed_est = std::make_shared<cvedix_nodes::cvedix_ba_line_speed_estimation_node>(
        "speed", line_pairs, pixel_to_meter, 80.0, true, false
    );

    // === 5. Accident Detection ===
    auto accident = std::make_shared<cvedix_nodes::cvedix_ba_accident_detection_node>(
        "accident", 0.15f, 3.0f, 60.0f, 15
    );

    // === 6. OSD ===
    auto osd = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_osd_node>("osd");

    // === 7. File output ===
    std::experimental::filesystem::create_directories("./output");
    auto file_des = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
        "file_out", 0, "./output", "traffic_", 10,
        cvedix_objects::cvedix_size(), 2048, true
    );

    // === Pipeline ===
    // file_src → detector → tracker → speed → accident → osd → file_des
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    speed_est->attach_to({tracker});
    accident->attach_to({speed_est});
    osd->attach_to({accident});
    file_des->attach_to({osd});

    CVEDIX_INFO("========================================");
    CVEDIX_INFO("  Traffic Analysis (Speed + Accident)");
    CVEDIX_INFO("========================================");
    CVEDIX_INFO("  Video:    " + video_path);
    CVEDIX_INFO("  Classes:  person,bicycle,car,motorcycle,bus,truck");
    CVEDIX_INFO("  Speed:    entry=y400, exit=y550, limit=80km/h");
    CVEDIX_INFO("  Accident: collision + sudden_stop + swerve");
    CVEDIX_INFO("  Output:   ./output/traffic_*.mp4");
    CVEDIX_INFO("========================================");

    file_src->start();
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    file_src->detach_recursively();

    CVEDIX_INFO("=== TRAFFIC ANALYSIS COMPLETE ===");
}

#else
#include <iostream>
int main() {
    std::cerr << "Requires -DCVEDIX_WITH_TRT=ON" << std::endl;
    return 1;
}
#endif
