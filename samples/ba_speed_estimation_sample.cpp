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
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_ocsort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_speed_estimation_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <thread>

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Parse args
    std::string video_path = "./cvedix_data/test_video/0206.mp4";
    std::string engine_path = "./cvedix_data/models/yolov11n.engine";
    if (argc > 1) video_path = argv[1];
    if (argc > 2) engine_path = argv[2];

    // === 1. Source: video file ===
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0,
        video_path,
        1.0,  // full resolution for 1080p
        false // no loop
    );

    // === 2. Detector: TensorRT YOLOv11 (vehicles only) ===
    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
      "yolo_detector",
      "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
      "./cvedix_data/models/yolov11/onnx/labels.txt",
      0.45, 0.5, 0, cvedix_nodes::BackendType::ONNX);

    // motorcycle(3), car(2), bus(5), truck(7)
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
    // For 1920x1080 video:
    // Line 1 (entry): upper detection line
    // Line 2 (exit):  lower detection line
    // Calibration depends on actual camera setup

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

    // Pixel-to-meter calibration (estimate: 150px ≈ 15m for typical road camera)
    std::map<int, double> pixel_to_meter = {
        {0, 15.0 / 150.0}  // 15 meters / 150 pixels
    };

    double speed_limit_kmh = 80.0;  // City road speed limit

    auto speed_est = std::make_shared<cvedix_nodes::cvedix_ba_line_speed_estimation_node>(
        "speed_estimation",
        line_pairs,
        pixel_to_meter,
        speed_limit_kmh,
        true,   // record image on violation
        false   // don't record video
    );

    // === 5. OSD: draw detection lines and speed on frame ===
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
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
    CVEDIX_INFO("=== Speed Estimation ===");
    CVEDIX_INFO("  Video: " + video_path);
    CVEDIX_INFO("  Classes: motorcycle, car, bus, truck");
    CVEDIX_INFO("  Line 1 (entry): y=400");
    CVEDIX_INFO("  Line 2 (exit):  y=550");
    CVEDIX_INFO("  Speed limit: 80 km/h");
    CVEDIX_INFO("  Output: ./output/speed_*.mp4");
    CVEDIX_INFO("=========================");

    file_src->start();

    // Auto-stop after video duration + 5s buffer
    std::this_thread::sleep_for(std::chrono::seconds(20));
    file_src->detach_recursively();
    CVEDIX_INFO("=== DONE ===");
}

#else

#include <iostream>
int main() {
    std::cerr << "This sample requires TensorRT. Rebuild with -DCVEDIX_WITH_TRT=ON" << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_TRT
