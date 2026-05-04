/**
 * @file ba_vehicle_running_red_light_sample.cpp
 * @brief Red light violation detection using 3 parallel TensorRT models
 *
 * Uses 3 YOLO11 TensorRT engines simultaneously:
 *   1. yolo11n.engine         - Vehicle detection (person, bicycle, car, motorcycle, bus, truck)
 *   2. traffic_sign_detector  - Traffic signal detection (Green/Red Light, Speed Limits, Stop)
 *   3. license-plate-finetune - License plate detection
 *
 * LINE MODE: crossing the stop line during RED = violation.
 *
 * Pipeline:
 *                          +-> vehicle_detector  (yolo11n TRT) -------+
 *   file_src -> split_in --+-> traffic_sign_detector (TRT) -----------+-> sync(MERGE) -> tracker -> signal_bridge -> ba_red_light -> osd -> web_debug
 *                          +-> plate_detector (TRT) ------------------+
 *
 * Requires: -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_GSTREAMER=ON
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_sync_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_red_light_violation_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>

// Signal Bridge Node
namespace {

class signal_bridge_node : public cvedix_nodes::cvedix_node {
public:
    signal_bridge_node(
        const std::string& name,
        std::shared_ptr<cvedix_nodes::cvedix_ba_line_red_light_violation_node> red_light_node,
        int green_class_id = 200,
        int red_class_id = 201,
        float signal_conf_threshold = 0.45f,
        int consecutive_frames = 3)
        : cvedix_node(name),
          red_light_node_(red_light_node),
          green_class_id_(green_class_id),
          red_class_id_(red_class_id),
          signal_conf_threshold_(signal_conf_threshold),
          consecutive_frames_needed_(consecutive_frames) {
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] signal_bridge: green_id=%d, red_id=%d, conf=%.2f, consecutive=%d",
            name.c_str(), green_class_id, red_class_id,
            signal_conf_threshold, consecutive_frames));
        this->initialized();
    }

    ~signal_bridge_node() { deinitialized(); }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {

        int ch = meta->channel_index;
        bool found_red = false;
        bool found_green = false;
        float best_conf = 0.0f;

        for (const auto& target : meta->targets) {
            int cls = target->primary_class_id;
            float conf = target->primary_score;
            if (conf < signal_conf_threshold_) continue;
            if (cls == red_class_id_) {
                found_red = true;
                if (conf > best_conf) best_conf = conf;
            } else if (cls == green_class_id_) {
                found_green = true;
                if (conf > best_conf) best_conf = conf;
            }
        }

        cvedix_nodes::traffic_signal_state candidate;
        if (found_red && !found_green) {
            candidate = cvedix_nodes::traffic_signal_state::RED;
        } else if (found_green && !found_red) {
            candidate = cvedix_nodes::traffic_signal_state::GREEN;
        } else if (found_red && found_green) {
            candidate = cvedix_nodes::traffic_signal_state::RED;
        } else {
            return meta;
        }

        if (candidate == last_candidate_[ch]) {
            consecutive_count_[ch]++;
        } else {
            last_candidate_[ch] = candidate;
            consecutive_count_[ch] = 1;
        }

        if (consecutive_count_[ch] >= consecutive_frames_needed_) {
            auto current = red_light_node_->get_signal_state(ch);
            if (current != candidate) {
                red_light_node_->set_signal_state(ch, candidate);
                std::string state_str =
                    (candidate == cvedix_nodes::traffic_signal_state::RED) ? "RED" :
                    (candidate == cvedix_nodes::traffic_signal_state::GREEN) ? "GREEN" : "UNKNOWN";
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] Signal auto-detected: %s (conf=%.2f, frames=%d)",
                    node_name.c_str(), ch, state_str.c_str(),
                    best_conf, consecutive_count_[ch]));
            }
        }

        return meta;
    }

private:
    std::shared_ptr<cvedix_nodes::cvedix_ba_line_red_light_violation_node> red_light_node_;
    int green_class_id_;
    int red_class_id_;
    float signal_conf_threshold_;
    int consecutive_frames_needed_;
    std::map<int, cvedix_nodes::traffic_signal_state> last_candidate_;
    std::map<int, int> consecutive_count_;
};

struct AppConfig {
    std::string video_path = "./cvedix_data/videos/red_light_running_compilation.mp4";
    int web_port = 9091;
    float resize_ratio = 0.5f;
    int skip_interval = 5;  // 60fps video → ~10fps effective (3 TRT models need ~100ms total)
    bool cycle = true;
};

AppConfig parse_args(int argc, char** argv) {
    AppConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--video" && i + 1 < argc)    cfg.video_path = argv[++i];
        else if (arg == "--port" && i + 1 < argc)    cfg.web_port = std::stoi(argv[++i]);
        else if (arg == "--resize" && i + 1 < argc) cfg.resize_ratio = std::stof(argv[++i]);
        else if (arg == "--skip" && i + 1 < argc)   cfg.skip_interval = std::stoi(argv[++i]);
        else if (arg == "--no-cycle")                cfg.cycle = false;
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "  --video <path>    Video file path\n"
                      << "  --port <num>      Web debug port (default: 9091)\n"
                      << "  --resize <ratio>  Resize ratio (default: 0.5)\n"
                      << "  --skip <n>        Skip interval (default: 2)\n"
                      << "  --no-cycle        Don't loop the video\n"
                      << "  --help            Show this help\n";
            exit(0);
        }
    }
    return cfg;
}

}  // anonymous namespace

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    std::vector<std::string> debug_keywords = {"signal_bridge", "red_light"};
    CVEDIX_SET_LOG_KEYWORDS_FOR_DEBUG(debug_keywords);
    CVEDIX_LOGGER_INIT();

    auto cfg = parse_args(argc, argv);

    // Model paths
    const std::string vehicle_engine = "./cvedix_data/models/yolo11n.engine";
    const std::string vehicle_labels = "./cvedix_data/models/yolov11/tensorrt/labels.txt";
    const std::string traffic_sign_engine = "./cvedix_data/models/traffic_sign_detector.engine";
    const std::string traffic_sign_labels = "./cvedix_data/models/traffic_sign_labels.txt";
    const std::string plate_engine = "./cvedix_data/models/license-plate-finetune-v1n.engine";
    const std::string plate_labels = "./cvedix_data/models/license_plate_labels.txt";

    // 1. Video File Source
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0,
        cfg.video_path,
        cfg.resize_ratio,
        cfg.cycle,
        "avdec_h264",
        cfg.skip_interval
    );

    // 2. Input Split: deep copy for 3 parallel branches
    auto split_input = std::make_shared<cvedix_nodes::cvedix_split_node>(
        "split_input", false, true);

    // 3. Branch A: Vehicle Detection (COCO classes)
    auto vehicle_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "vehicle_detector",
        vehicle_engine,
        cvedix_nodes::YoloVersion::YOLO11,
        vehicle_labels,
        0.45f, 0.5f, 0,
        cvedix_nodes::BackendType::TENSORRT
    );
    vehicle_detector->set_allowed_classes({0, 1, 2, 3, 5, 7});

    // 4. Branch B: Traffic Sign Detection
    auto traffic_sign_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "traffic_sign_detector",
        traffic_sign_engine,
        cvedix_nodes::YoloVersion::YOLO11,
        traffic_sign_labels,
        0.35f, 0.45f, 200,
        cvedix_nodes::BackendType::TENSORRT
    );

    // 5. Branch C: License Plate Detection
    auto plate_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "plate_detector",
        plate_engine,
        cvedix_nodes::YoloVersion::YOLO11,
        plate_labels,
        0.40f, 0.5f, 300,
        cvedix_nodes::BackendType::TENSORRT
    );

    // 6. Sync: Merge all 3 branches
    auto sync = std::make_shared<cvedix_nodes::cvedix_sync_node>(
        "sync_merge",
        cvedix_nodes::cvedix_sync_mode::MERGE,
        2000
    );

    // 7. Tracker: ByteTrack
    auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
        "tracker",
        cvedix_nodes::cvedix_track_for::NORMAL,
        0.3f, 0.6f, 0.7f, 60, 15
    );

    // 8. Red Light Violation Detection (LINE MODE)
    //
    // LINE-ONLY mode: crossing the stop line during RED = violation.
    // The intersection area is a thin 5px strip right above the stop line
    // so that crossing the line immediately triggers a RED_LIGHT violation.
    //
    // Frame is 960x540 (1920x1080 @ 0.5 resize)
    //
    // Stop line: horizontal line across the frame at y=350
    cvedix_objects::cvedix_line stop_line(
        cvedix_objects::cvedix_point(20, 350),
        cvedix_objects::cvedix_point(940, 350)
    );

    // Intersection area: thin 5px strip above stop line
    // (crossing stop line = entering this strip = immediate violation)
    std::vector<cvedix_objects::cvedix_point> intersection_area = {
        {20, 345}, {940, 345}, {940, 350}, {20, 350}
    };

    std::set<int> monitored_vehicles = {0, 1, 2, 3, 5, 7};

    cvedix_nodes::red_light_config rl_cfg(
        stop_line, intersection_area, monitored_vehicles,
        1.0, "Stop Line", cv::Scalar(0, 0, 255)
    );

    std::map<int, cvedix_nodes::red_light_config> rl_configs = {{0, rl_cfg}};
    auto ba_red_light = std::make_shared<cvedix_nodes::cvedix_ba_line_red_light_violation_node>(
        "ba_red_light", rl_configs, true, true);

    // 9. Signal Bridge: auto-detect signal state from traffic sign model
    auto signal_bridge = std::make_shared<signal_bridge_node>(
        "signal_bridge", ba_red_light,
        200, 201, 0.45f, 3
    );

    // 10. OSD
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd_cfg.show_track_id = true;
    osd_cfg.show_track_trail = true;
    osd_cfg.show_center_dot = true;
    osd_cfg.show_static_lines = true;
    osd_cfg.show_static_zones = false;
    osd_cfg.enable_ba_crossline = true;
    osd_cfg.enable_ba_enter_exit = true;
    osd_cfg.enable_ba_crowding = false;
    osd_cfg.enable_ba_jam = false;
    osd_cfg.enable_ba_stop = false;
    osd_cfg.enable_plate = true;
    osd_cfg.label_font_scale = 0.5;
    osd_cfg.bbox_thickness = 2;
    osd_cfg.alert_color = cv::Scalar(0, 0, 255);
    osd->update_config(osd_cfg);

    std::vector<cvedix_nodes::unified_static_line_config> static_lines = {
        cvedix_nodes::unified_static_line_config(stop_line, cv::Scalar(0, 0, 255), "STOP LINE")
    };
    osd->set_static_lines(static_lines);

    // 11. Output: Web Debug
    auto web_des = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, cfg.web_port, nullptr, 60);

    // Build Pipeline
    split_input->attach_to({file_src});
    vehicle_detector->attach_to({split_input});
    traffic_sign_detector->attach_to({split_input});
    plate_detector->attach_to({split_input});
    sync->attach_to({vehicle_detector, traffic_sign_detector, plate_detector});
    tracker->attach_to({sync});
    signal_bridge->attach_to({tracker});
    ba_red_light->attach_to({signal_bridge});
    osd->attach_to({ba_red_light});
    web_des->attach_to({osd});

    // Analysis Board
    auto board = std::make_unique<cvedix_utils::cvedix_analysis_board>(
        std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>{file_src});
    board->push_to_buffer(5);
    web_des->set_board(board.get());

    // Start
    CVEDIX_INFO("================================================================");
    CVEDIX_INFO("  Red Light Violation Sample (LINE MODE)");
    CVEDIX_INFO("================================================================");
    CVEDIX_INFO("  Video: " + cfg.video_path);
    CVEDIX_INFO("  Web:   http://localhost:" + std::to_string(cfg.web_port));
    CVEDIX_INFO("  Mode:  Cross line on RED = violation");
    CVEDIX_INFO("  Line:  (20,350) -> (940,350)");
    CVEDIX_INFO("================================================================");

    std::cout << "\nPress Enter to stop...\n" << std::endl;

    file_src->start();

    std::string wait;
    std::getline(std::cin, wait);

    file_src->detach_recursively();
    CVEDIX_INFO("=== RED LIGHT DETECTION STOPPED ===");
    return 0;
}

#else
#include <iostream>
int main() {
    std::cerr << "This sample requires -DCVEDIX_WITH_TRT=ON and -DCVEDIX_WITH_GSTREAMER=ON" << std::endl;
    return 1;
}
#endif
