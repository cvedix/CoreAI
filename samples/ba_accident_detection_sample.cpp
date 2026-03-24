/**
 * @file ba_accident_detection_sample.cpp
 * @brief Sample: Traffic accident detection using tracked object analysis
 *
 * Pipeline:
 *   file_src → detector → tracker → accident_detection → osd → file_des
 *
 * Detects:
 *   - Vehicle collisions (bbox overlap)
 *   - Sudden stops (moving → stopped)
 *   - Swerving (trajectory direction change)
 *
 * Requires: -DCVEDIX_WITH_TRT=ON
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_det_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/ba/cvedix_ba_accident_detection_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"

#include <thread>

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Parse args
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

    // === 2. Detector: YOLOv11 (vehicles + motorcycle) ===
    auto detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_det_node>(
        "detector", engine_path,
        "./cvedix_data/models/coco_80_labels_list.txt",
        0.15f, 0.45f
    );
    // COCO: 0=person, 1=bicycle, 2=car, 3=motorcycle, 5=bus, 7=truck
    detector->set_allowed_classes({0, 1, 2, 3, 5, 7});

    // === 3. Tracker: ByteTrack (optimized for stability) ===
    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "tracker",
        cvedix_nodes::cvedix_track_for::NORMAL,
        0.1f,   // track_thresh
        0.5f,   // high_thresh
        0.7f,   // match_thresh
        90,     // track_buffer
        30      // frame_rate (0206.mp4 = 30fps)
    );

    // === 4. Accident Detection ===
    auto accident = std::make_shared<cvedix_nodes::cvedix_ba_accident_detection_node>(
        "accident",
        0.15f,  // collision IoU threshold (15% overlap = potential collision)
        3.0f,   // min moving speed (px/frame)
        60.0f,  // swerve angle threshold (degrees)
        15      // history frames to analyze
    );

    // === 5. OSD ===
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

    // === 6. File output ===
    std::experimental::filesystem::create_directories("./output");
    auto file_des = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
        "file_out", 0,
        "./output",
        "accident_",
        10,
        cvedix_objects::cvedix_size(),
        2048,
        true
    );

    // === Build pipeline ===
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    accident->attach_to({tracker});
    osd->attach_to({accident});
    file_des->attach_to({osd});

    // === Start ===
    CVEDIX_INFO("========================================");
    CVEDIX_INFO("  Traffic Accident Detection");
    CVEDIX_INFO("========================================");
    CVEDIX_INFO("  Video:     " + video_path);
    CVEDIX_INFO("  Engine:    " + engine_path);
    CVEDIX_INFO("  Duration:  " + std::to_string(duration) + "s");
    CVEDIX_INFO("  Classes:   person, bicycle, car, motorcycle, bus, truck");
    CVEDIX_INFO("  Detection: collision + sudden_stop + swerve");
    CVEDIX_INFO("  Output:    ./output/accident_*.mp4");
    CVEDIX_INFO("========================================");

    file_src->start();

    std::this_thread::sleep_for(std::chrono::seconds(duration));
    file_src->detach_recursively();

    CVEDIX_INFO("========================================");
    CVEDIX_INFO("  ACCIDENT DETECTION COMPLETE");
    CVEDIX_INFO("========================================");
}

#else

#include <iostream>
int main() {
    std::cerr << "This sample requires TensorRT. Rebuild with -DCVEDIX_WITH_TRT=ON" << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_TRT
