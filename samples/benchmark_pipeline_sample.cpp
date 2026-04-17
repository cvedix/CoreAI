/**
 * @file benchmark_pipeline_sample.cpp
 * @brief Benchmark: Detection → Tracking → BA Line Crossing pipeline
 *
 * Measures end-to-end throughput of a complete analytics pipeline
 * using TensorRT YOLOv11 detection, with video output.
 *
 * Pipeline:
 *   file_src → det_node → tracker → ba_crossline → osd → file_des
 *
 * Usage:
 *   ./benchmark_pipeline_sample [engine] [video] [duration_sec]
 *
 * Requires: -DCVEDIX_WITH_TRT=ON
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_hybrid_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <experimental/filesystem>
#include <iostream>
#include <thread>

std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // --- Parse args ---
    std::string engine_path = "./cvedix_data/models/yolov11n.engine";
    std::string video_path = "./cvedix_data/test_video/vehicle_count.mp4";
    int duration_sec = 30;

    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        std::cout << "Usage: " << argv[0] << " [engine] [video] [duration_sec]" << std::endl;
        return 0;
    }
    if (argc > 1) engine_path = argv[1];
    if (argc > 2) video_path = argv[2];
    if (argc > 3) duration_sec = std::stoi(argv[3]);

    CVEDIX_INFO("========================================");
    CVEDIX_INFO("  Pipeline Benchmark (with video output)");
    CVEDIX_INFO("========================================");
    CVEDIX_INFO("Engine:   " + engine_path);
    CVEDIX_INFO("Video:    " + video_path);
    CVEDIX_INFO("Duration: " + std::to_string(duration_sec) + "s");
    CVEDIX_INFO("Output:   ./output/benchmark_*.mp4");
    CVEDIX_INFO("========================================");

    try {
        // === 1. Source ===
        auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            "src", 0, video_path, 1.0f, false  // no loop, process video once
        );

        // === 2. Detector ===
        auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
                        "yolo_detector",
                        "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
                        cvedix_nodes::YoloVersion::YOLO11,                      // use YOLOv11 plugin family
                        "./cvedix_data/models/yolov11/onnx/labels.txt",
                        0.45, 0.5, 0, cvedix_nodes::BackendType::ONNX);
        detector->set_allowed_classes({2, 3, 5, 7}); // car, motorbike, bus, truck

        // === 3. Tracker (Hybrid: ByteTrack + KCF fallback) ===
        auto tracker = std::make_shared<cvedix_nodes::cvedix_hybrid_track_node>(
            "tracker",
            cvedix_nodes::cvedix_track_for::NORMAL,
            0.1f,   // track_thresh
            0.5f,   // high_thresh
            0.7f,   // match_thresh
            90,     // track_buffer (ByteTrack)
            25,     // frame_rate
            150     // kcf_max_frames (KCF keeps track up to 6s)
        );

        // === 4. BA Line Crossing ===
        cvedix_objects::cvedix_line crossline(
            cvedix_objects::cvedix_point(0, 500),
            cvedix_objects::cvedix_point(1920, 500)
        );
        std::map<int, std::vector<cvedix_objects::cvedix_line>> lines = {
            {0, {crossline}}
        };
        auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>(
            "crossline", lines, false, false
        );

        // === 5. OSD: draw bboxes, tracks, crossline on frame ===
        auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

        // === 6. File output (MP4) ===
        std::experimental::filesystem::create_directories("./output");
        auto file_des = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
            "file_out", 0,
            "./output",           // save directory
            "benchmark_stable_",  // filename prefix
            10,                   // max 10 minutes per file
            cvedix_objects::cvedix_size(),  // auto resolution
            2048,                 // bitrate (kbps)
            true                  // OSD enabled
        );

        // === Build Pipeline ===
        //  src → detector → tracker → crossline → osd → file_des
        detector->attach_to({src});
        tracker->attach_to({detector});
        ba_crossline->attach_to({tracker});
        osd->attach_to({ba_crossline});
        file_des->attach_to({osd});

        // === Start ===
        CVEDIX_INFO("Pipeline: src → detector → tracker → crossline → osd → file_des");
        CVEDIX_INFO("Press Ctrl+C to stop early...");

        auto bench_start = std::chrono::steady_clock::now();
        src->start();

        // Analysis Board for per-node stats
        cvedix_utils::cvedix_analysis_board board({src});
        board.display(5, false);

        // Run for duration or until video ends
        while (g_running) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - bench_start).count();
            if (elapsed >= duration_sec) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        auto bench_end = std::chrono::steady_clock::now();
        double total_time = std::chrono::duration<double>(bench_end - bench_start).count();

        src->detach_recursively();

        CVEDIX_INFO("========================================");
        CVEDIX_INFO("         BENCHMARK COMPLETE");
        CVEDIX_INFO("========================================");
        CVEDIX_INFO("  Duration: " + std::to_string(total_time).substr(0, 5) + " s");
        CVEDIX_INFO("  Output:   ./output/benchmark_*.mp4");
        CVEDIX_INFO("========================================");

    } catch (const std::exception& e) {
        CVEDIX_ERROR("Exception: " + std::string(e.what()));
        return -1;
    }

    return 0;
}

#else

#include <iostream>
int main() {
    std::cerr << "This benchmark requires TensorRT." << std::endl;
    std::cerr << "Rebuild with: -DCVEDIX_WITH_TRT=ON" << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_TRT
