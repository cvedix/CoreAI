/**
 * @file benchmark_people_analytics_sample.cpp
 * @brief Benchmark: People Counting + Crowd + Loitering + Wrong-Way pipeline
 *
 * Single shared detection+tracking front-end with 4 BA sub-pipelines chained
 * linearly. Outputs video + CSV report.
 *
 * Pipeline (linear chain):
 *   file_src → detector → tracker → ba_counting → ba_crowding
 *     → ba_loitering → ba_wrong_way → osd_counting → osd_crowding → file_des
 *
 * Usage:
 *   ./benchmark_people_analytics_sample [engine] [video] [duration_sec]
 *
 * Output:
 *   - ./output/benchmark_people_*.mp4   (video with OSD)
 *   - ./output/benchmark_people_report.csv  (event log, opens in Excel)
 *
 * Requires: -DCVEDIX_WITH_TRT=ON
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_hybrid_track_node.h"

// BA nodes
#include "cvedix/nodes/ba/cvedix_ba_line_counting_node.h"
#include "cvedix/nodes/ba/cvedix_ba_area_crowding_node.h"
#include "cvedix/nodes/ba/cvedix_ba_area_loitering_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_wrong_way_node.h"

// OSD nodes
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

// Output
#include "cvedix/nodes/des/cvedix_file_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <experimental/filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

// ── CSV Event Logger ──────────────────────────────────────────────────
struct csv_event {
    std::string timestamp;
    int         frame_no;
    std::string event_type;  // COUNTING / CROWDING / LOITERING / WRONG_WAY
    std::string details;
};

static std::mutex              g_csv_mutex;
static std::vector<csv_event>  g_csv_events;
static int                     g_frame_counter = 0;

static std::string now_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;
    std::tm tm;
    localtime_r(&t, &tm);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

static void log_event(const std::string& type, const std::string& detail) {
    std::lock_guard<std::mutex> lock(g_csv_mutex);
    g_csv_events.push_back({now_timestamp(), g_frame_counter, type, detail});
}

static void write_csv(const std::string& path) {
    std::ofstream f(path);
    // BOM for Excel UTF-8
    f << "\xEF\xBB\xBF";
    f << "Timestamp,Frame,Event Type,Details\n";
    for (auto& e : g_csv_events) {
        // Escape quotes in details
        std::string safe = e.details;
        for (size_t p = 0; (p = safe.find('"', p)) != std::string::npos; p += 2)
            safe.insert(p, 1, '"');
        f << e.timestamp << "," << e.frame_no << "," << e.event_type
          << ",\"" << safe << "\"\n";
    }
    f.close();
}

// ── Custom BA callback node ───────────────────────────────────────────
// Lightweight passthrough node that watches BA results on each frame
// and logs events to the CSV collector.
#include "cvedix/nodes/common/cvedix_node.h"

class ba_event_logger_node : public cvedix_nodes::cvedix_node {
protected:
    std::shared_ptr<cvedix_objects::cvedix_meta>
    handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        if (!meta) return meta;
        g_frame_counter = meta->frame_index;

        // Check BA results
        for (auto& ba : meta->ba_results) {
            if (!ba) continue;
            std::string label = ba->ba_label;
            std::string detail;
            int ba_type_val = static_cast<int>(ba->type);

            if (label == "crossline" || label == "counting") {
                detail = "type=" + std::to_string(ba_type_val)
                       + " targets=" + std::to_string(ba->involve_target_ids_in_frame.size());
                log_event("COUNTING", detail);
            }
            else if (label == "crowding") {
                detail = "people_count=" + std::to_string(ba->involve_target_ids_in_frame.size())
                       + " type=" + std::to_string(ba_type_val);
                log_event("CROWDING", detail);
            }
            else if (label == "loitering") {
                detail = "track_ids=[";
                for (size_t i = 0; i < ba->involve_target_ids_in_frame.size(); ++i) {
                    if (i > 0) detail += ",";
                    detail += std::to_string(ba->involve_target_ids_in_frame[i]);
                }
                detail += "] type=" + std::to_string(ba_type_val);
                log_event("LOITERING", detail);
            }
            else if (label == "wrong_way" || label == "direction_violation") {
                detail = "track_ids=[";
                for (size_t i = 0; i < ba->involve_target_ids_in_frame.size(); ++i) {
                    if (i > 0) detail += ",";
                    detail += std::to_string(ba->involve_target_ids_in_frame[i]);
                }
                detail += "]";
                log_event("WRONG_WAY", detail);
            }
            else {
                // Log any other BA event
                detail = "label=" + label + " type=" + std::to_string(ba_type_val);
                log_event(label, detail);
            }
        }

        // Log periodic summary every 100 frames
        if (meta->frame_index > 0 && meta->frame_index % 100 == 0) {
            int persons = static_cast<int>(meta->targets.size());
            log_event("SUMMARY", "frame=" + std::to_string(meta->frame_index)
                     + " persons_detected=" + std::to_string(persons));
        }

        return meta;
    }

public:
    ba_event_logger_node(std::string name) : cvedix_node(name) {}
    ~ba_event_logger_node() = default;
};


int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);

    // Force offscreen Qt to avoid xcb crash on headless servers
    setenv("QT_QPA_PLATFORM", "offscreen", 0);

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // --- Parse args ---
    std::string engine_path = "./cvedix_data/models/yolov11n.engine";
    std::string video_path  = "./cvedix_data/test_video/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4";
    int duration_sec = 60;

    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        std::cout << "Usage: " << argv[0] << " [engine] [video] [duration_sec]" << std::endl;
        std::cout << std::endl;
        std::cout << "Sub-pipelines:" << std::endl;
        std::cout << "  1. People counting  (line crossing)" << std::endl;
        std::cout << "  2. Crowd detection  (area threshold)" << std::endl;
        std::cout << "  3. Loitering        (area dwell time)" << std::endl;
        std::cout << "  4. Wrong-way        (direction violation)" << std::endl;
        std::cout << std::endl;
        std::cout << "Output:" << std::endl;
        std::cout << "  - ./output/benchmark_people_*.mp4          (video)" << std::endl;
        std::cout << "  - ./output/benchmark_people_report.csv     (Excel)" << std::endl;
        return 0;
    }
    if (argc > 1) engine_path = argv[1];
    if (argc > 2) video_path  = argv[2];
    if (argc > 3) duration_sec = std::stoi(argv[3]);

    CVEDIX_INFO("==============================================");
    CVEDIX_INFO("  People Analytics Benchmark Pipeline");
    CVEDIX_INFO("==============================================");
    CVEDIX_INFO("Engine:   " + engine_path);
    CVEDIX_INFO("Video:    " + video_path);
    CVEDIX_INFO("Duration: " + std::to_string(duration_sec) + "s");
    CVEDIX_INFO("Video:    ./output/benchmark_people_*.mp4");
    CVEDIX_INFO("Report:   ./output/benchmark_people_report.csv");
    CVEDIX_INFO("==============================================");

    try {
        // ==========================================================
        // 1. SHARED FRONT-END: Source → Detector → Tracker
        // ==========================================================

        auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            "src", 0, video_path, 1.0f, false
        );

        auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
            "detector", engine_path,
            "./cvedix_data/models/coco_80_labels_list.txt",
            0.25f, 0.45f
        );
        detector->set_allowed_classes({0}); // person only

        auto tracker = std::make_shared<cvedix_nodes::cvedix_hybrid_track_node>(
            "tracker",
            cvedix_nodes::cvedix_track_for::NORMAL,
            0.1f,   // track_thresh
            0.5f,   // high_thresh
            0.7f,   // match_thresh
            90,     // track_buffer
            25,     // frame_rate
            150     // kcf_max_frames
        );

        // ==========================================================
        // 2. BA SUB-PIPELINE 1: People Counting (Line Crossing)
        // ==========================================================
        CVEDIX_INFO("[BA-1] People Counting: horizontal line at y=400");

        cvedix_nodes::cvedix_ba_line_couting_setting count_line;
        count_line.setting_name = "count_line";
        count_line.line = cvedix_objects::cvedix_line(
            cvedix_objects::cvedix_point(0, 400),
            cvedix_objects::cvedix_point(1920, 400)
        );
        count_line.direction = cvedix_objects::cvedix_ba_direct_type::BOTH;

        std::map<int, std::vector<cvedix_nodes::cvedix_ba_line_couting_setting>> count_settings = {
            {0, {count_line}}
        };
        auto ba_counting = std::make_shared<cvedix_nodes::cvedix_ba_line_counting_node>(
            "ba_counting", count_settings, false, false
        );

        auto osd_counting = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_counting");

        // ==========================================================
        // 3. BA SUB-PIPELINE 2: Crowd Detection
        // ==========================================================
        CVEDIX_INFO("[BA-2] Crowd Detection: polygon ROI, threshold=5, alarm=3s");

        std::map<int, std::vector<cvedix_objects::cvedix_point>> crowd_rois = {
            {0, {
                cvedix_objects::cvedix_point(100, 200),
                cvedix_objects::cvedix_point(1800, 200),
                cvedix_objects::cvedix_point(1800, 900),
                cvedix_objects::cvedix_point(100, 900)
            }}
        };
        std::map<int, cvedix_nodes::crowding_config> crowd_configs = {
            {0, cvedix_nodes::crowding_config(5, 3.0, "Main Area")}
        };
        auto ba_crowding = std::make_shared<cvedix_nodes::cvedix_ba_area_crowding_node>(
            "ba_crowding", crowd_rois, crowd_configs, 25, false, false
        );

        auto osd_crowding = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_crowding");

        // ==========================================================
        // 4. BA SUB-PIPELINE 3: Loitering Detection
        // ==========================================================
        CVEDIX_INFO("[BA-3] Loitering: polygon ROI, alarm=15s");

        std::map<int, std::vector<cvedix_objects::cvedix_point>> loiter_rois = {
            {0, {
                cvedix_objects::cvedix_point(200, 300),
                cvedix_objects::cvedix_point(1700, 300),
                cvedix_objects::cvedix_point(1700, 800),
                cvedix_objects::cvedix_point(200, 800)
            }}
        };
        std::map<int, cvedix_nodes::loitering_config> loiter_configs = {
            {0, cvedix_nodes::loitering_config(15.0, "Watch Zone", cv::Scalar(0, 165, 255))}
        };
        auto ba_loitering = std::make_shared<cvedix_nodes::cvedix_ba_area_loitering_node>(
            "ba_loitering", loiter_rois, loiter_configs, 25, false, false
        );

        // ==========================================================
        // 5. BA SUB-PIPELINE 4: Wrong-Way Detection
        // ==========================================================
        CVEDIX_INFO("[BA-4] Wrong-Way: 2 parallel lines, allowed=IN (upward)");

        cvedix_objects::cvedix_line ww_line1(
            cvedix_objects::cvedix_point(0, 350),
            cvedix_objects::cvedix_point(1920, 350)
        );
        cvedix_objects::cvedix_line ww_line2(
            cvedix_objects::cvedix_point(0, 550),
            cvedix_objects::cvedix_point(1920, 550)
        );
        cvedix_nodes::wrong_way_config ww_cfg(
            {ww_line1, ww_line2},
            cvedix_objects::cvedix_ba_direct_type::IN,  // only upward allowed
            2,   // min lines crossed to confirm
            {},  // no exempt classes
            "Wrong-Way Zone",
            cv::Scalar(0, 0, 255)  // Red
        );
        std::map<int, cvedix_nodes::wrong_way_config> ww_configs = {
            {0, ww_cfg}
        };
        auto ba_wrong_way = std::make_shared<cvedix_nodes::cvedix_ba_line_wrong_way_node>(
            "ba_wrong_way", ww_configs, false, false
        );

        // ==========================================================
        // 6. Event Logger (collects BA events → CSV)
        // ==========================================================
        auto event_logger = std::make_shared<ba_event_logger_node>("event_logger");

        // ==========================================================
        // 7. File Output
        // ==========================================================
        std::experimental::filesystem::create_directories("./output");
        auto file_des = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
            "file_out", 0,
            "./output",
            "benchmark_people_",
            10,                                // max 10 minutes per file
            cvedix_objects::cvedix_size(),      // auto resolution
            2048,                               // bitrate (kbps)
            true                                // OSD enabled
        );

        // ==========================================================
        // BUILD PIPELINE (linear chain — all BA nodes in series)
        // ==========================================================
        //  src → detector → tracker → ba_counting → ba_crowding
        //    → ba_loitering → ba_wrong_way → event_logger
        //    → osd_counting → osd_crowding → file_des

        // Shared front-end
        detector->attach_to({src});
        tracker->attach_to({detector});

        // Chain BA nodes sequentially
        ba_counting->attach_to({tracker});
        ba_crowding->attach_to({ba_counting});
        ba_loitering->attach_to({ba_crowding});
        ba_wrong_way->attach_to({ba_loitering});

        // Event logger (captures all BA results for CSV export)
        event_logger->attach_to({ba_wrong_way});

        // OSD chain (each draws its own BA results)
        osd_counting->attach_to({event_logger});
        osd_crowding->attach_to({osd_counting});

        // File output at the end
        file_des->attach_to({osd_crowding});

        // ==========================================================
        // START
        // ==========================================================
        CVEDIX_INFO("Pipeline built (linear chain):");
        CVEDIX_INFO("  src → detector → tracker → ba_counting → ba_crowding");
        CVEDIX_INFO("    → ba_loitering → ba_wrong_way → event_logger → osd → file_des");
        CVEDIX_INFO("Press Ctrl+C to stop early...");

        auto bench_start = std::chrono::steady_clock::now();
        src->start();

        // Analysis Board for per-node stats
        cvedix_utils::cvedix_analysis_board board({src});
        board.display();

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

        // ==========================================================
        // WRITE CSV REPORT
        // ==========================================================
        std::string csv_path = "./output/benchmark_people_report.csv";
        write_csv(csv_path);

        CVEDIX_INFO("==============================================");
        CVEDIX_INFO("     PEOPLE ANALYTICS BENCHMARK COMPLETE");
        CVEDIX_INFO("==============================================");
        CVEDIX_INFO("  Duration:  " + std::to_string(total_time).substr(0, 6) + " s");
        CVEDIX_INFO("  Video:     ./output/benchmark_people_*.mp4");
        CVEDIX_INFO("  CSV Report:" + csv_path);
        CVEDIX_INFO("  Events:    " + std::to_string(g_csv_events.size()));
        CVEDIX_INFO("==============================================");

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
