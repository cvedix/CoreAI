/**
 * @file ba_speed_estimation_sample.cpp
 * @brief Sample: Vehicle speed estimation using two detection lines (TensorRT)
 *
 * Pipeline:
 *   file_src → trt_yolov11_detector → tracker → speed_estimation → osd → file_des
 *
 * Requires: -DCVEDIX_WITH_TRT=ON
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_detector_node.h"
#include "cvedix/nodes/track/cvedix_ocsort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_speed_estimation_node.h"
#include "cvedix/nodes/osd/cvedix_ba_line_crossline_osd_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // === 1. Source: video file ===
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0,
        "./cvedix_data/test_video/vehicle_count.mp4",
        0.6,  // scale factor
        false // no loop, process video once
    );

    // === 2. Detector: TensorRT YOLOv11 (vehicles only) ===
    auto detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_detector_node>(
        "detector",
        "./cvedix_data/models/yolov11n.engine",          // TensorRT engine
        "./cvedix_data/models/coco_80_labels_list.txt",   // labels file
        0.15f,  // confidence threshold (lower for better recall)
        0.45f   // NMS threshold
    );

    // Filter to vehicle classes only (COCO 80 labels, 0-indexed):
    //   2=car, 3=motorbike, 5=bus, 7=truck
    detector->set_allowed_classes({2, 3, 5, 7});

    // === 3. Tracker: OC-SORT ===
    auto tracker = std::make_shared<cvedix_nodes::cvedix_ocsort_track_node>(
        "tracker",
        cvedix_nodes::cvedix_track_for::NORMAL,
        0.3,   // det_thresh (lower to keep more detections)
        30,    // max_age (longer memory for lost tracks)
        1,     // min_hits (faster track confirmation)
        0.2,   // iou_threshold (looser matching)
        3,     // delta_t
        "iou", // asso_func
        0.3,   // inertia (higher for smoother prediction)
        true   // use_byte
    );

    // === 4. Speed Estimation BA Node ===
    //
    // Video: highway camera (1920x1080 @ 25fps, scale=0.6 → 1152x648)
    // Vehicles move from far (top) to near (bottom) on left lanes.
    //
    // Line 1 (entry): at upper red road marking, y≈290
    // Line 2 (exit):  at lower red road marking, y≈460
    // X range: left lanes only (x=200 to x=530)
    //
    // Real-world calibration (highway):
    //   Distance between red markings ≈ 50 meters
    //   Pixel distance ≈ 170px
    //   pixel_to_meter = 50.0 / 170.0 ≈ 0.294

    cvedix_objects::cvedix_line line1(
        cvedix_objects::cvedix_point(120, 290),
        cvedix_objects::cvedix_point(570, 290)
    );
    cvedix_objects::cvedix_line line2(
        cvedix_objects::cvedix_point(50, 460),
        cvedix_objects::cvedix_point(640, 460)
    );

    std::map<int, std::pair<cvedix_objects::cvedix_line, cvedix_objects::cvedix_line>> line_pairs = {
        {0, {line1, line2}}  // channel 0: entry line, exit line
    };

    std::map<int, double> pixel_to_meter = {
        {0, 50.0 / 170.0}  // channel 0: 50 meters / 170 pixels (highway)
    };

    double speed_limit_kmh = 120.0;  // Highway speed limit: 120 km/h

    auto speed_est = std::make_shared<cvedix_nodes::cvedix_ba_line_speed_estimation_node>(
        "speed_estimation",
        line_pairs,
        pixel_to_meter,
        speed_limit_kmh,
        true,   // record image on violation
        false   // don't record video
    );

    // === 5. OSD: draw detection lines and speed on frame ===
    auto osd = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_osd_node>("osd");
    // Lines hidden from display (no set_line_configs call)

    // === 6. File output (MP4) ===
    std::experimental::filesystem::create_directories("./output");
    auto file_des = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
        "file_out", 0,
        "./output",         // save directory
        "speed_",           // filename prefix
        10,                 // max 10 minutes per file
        cvedix_objects::cvedix_size(),  // auto resolution
        2048,               // bitrate (kbps)
        true                // OSD enabled
    );

    // === Build pipeline ===
    //
    //  file_src → detector → tracker → speed_estimation → osd → file_des
    //
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    speed_est->attach_to({tracker});
    osd->attach_to({speed_est});
    file_des->attach_to({osd});

    // === Start ===
    CVEDIX_INFO("=== Speed Estimation Sample ===");
    CVEDIX_INFO("  Vehicle classes: car, motorbike, bus, truck");
    CVEDIX_INFO("  Line 1 (entry): y=290, x=[200, 530] (green)");
    CVEDIX_INFO("  Line 2 (exit):  y=460, x=[170, 570] (red)");
    CVEDIX_INFO("  Distance: 50 meters (highway)");
    CVEDIX_INFO("  Speed limit: 120 km/h");
    CVEDIX_INFO("  Output: ./output/speed_*.mp4");
    CVEDIX_INFO("  Press ENTER to stop...");
    CVEDIX_INFO("===============================");

    file_src->start();

    // Wait for user input to stop
    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
}

#else

#include <iostream>
int main() {
    std::cerr << "This sample requires TensorRT. Rebuild with -DCVEDIX_WITH_TRT=ON" << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_TRT
