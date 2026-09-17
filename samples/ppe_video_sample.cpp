#include "cvedix/nodes/src/cvedix_app_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_app_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/nodes/mid/cvedix_sync_node.h"
#include "cvedix/nodes/ba/cvedix_ba_ppe_safety_node.h"
#include "cvedix/nodes/broker/cvedix_ppe_event_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "ppe_mqtt_publisher.h"

#include <opencv2/videoio.hpp>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }

struct Options {
    std::string video = "data/videos/16.22.09.mp4";
    std::string model;
    std::string backend = "tensorrt";
    std::string labels = "configs/ppe_labels.txt";
    std::string output;
    std::string person_model;
    std::string tracking = "bytetrack";
    bool analysis_board = false;
    float person_conf = 0.35f;
    float conf = 0.25f, nms = 0.45f;
    int max_frames = 0;
    // HeraMind event publishing. Empty host (and no HERAMIND_MQTT_HOST) leaves
    // the event branch out of the pipeline entirely.
    std::string mqtt_host, mqtt_topic, camera_id, run_id, event_archive_dir;
    int mqtt_port = 1883;
    int confirm_frames = 3;
    int cooldown_ms = 10000;
};

/// Parse a decimal integer covering the whole value, or throw.
int parse_int(const std::string& arg, const std::string& value, int minimum) {
    size_t used = 0;
    int parsed = 0;
    try {
        parsed = std::stoi(value, &used);
    } catch (const std::exception&) {
        throw std::runtime_error(arg + " must be an integer");
    }
    if (used != value.size() || parsed < minimum)
        throw std::runtime_error(arg + " must be an integer >= " + std::to_string(minimum));
    return parsed;
}

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (i + 1 == argc) throw std::runtime_error("Missing value for " + arg);
        const std::string value = argv[++i];
        if (arg == "--video") o.video = value;
        else if (arg == "--model") o.model = value;
        else if (arg == "--backend") o.backend = value;
        else if (arg == "--labels") o.labels = value;
        else if (arg == "--output") o.output = value;
        else if (arg == "--person-model") o.person_model = value;
        else if (arg == "--tracking") o.tracking = value;
        else if (arg == "--analysis-board") {
            if (value != "0" && value != "1") throw std::runtime_error("--analysis-board must be 0 or 1");
            o.analysis_board = value == "1";
        }
        else if (arg == "--conf" || arg == "--nms" || arg == "--person-conf") {
            size_t used = 0;
            float threshold = std::stof(value, &used);
            if (used != value.size() || !std::isfinite(threshold) || threshold <= 0 || threshold > 1)
                throw std::runtime_error(arg + " must be in (0, 1]");
            (arg == "--conf" ? o.conf : arg == "--nms" ? o.nms : o.person_conf) = threshold;
        } else if (arg == "--max-frames") {
            o.max_frames = parse_int(arg, value, 0);
        }
        else if (arg == "--mqtt-host") o.mqtt_host = value;
        else if (arg == "--mqtt-port") o.mqtt_port = parse_int(arg, value, 1);
        else if (arg == "--mqtt-topic") o.mqtt_topic = value;
        else if (arg == "--camera-id") o.camera_id = value;
        else if (arg == "--run-id") o.run_id = value;
        else if (arg == "--event-archive-dir") o.event_archive_dir = value;
        else if (arg == "--confirm-frames") o.confirm_frames = parse_int(arg, value, 1);
        else if (arg == "--cooldown-ms") o.cooldown_ms = parse_int(arg, value, 0);
        else throw std::runtime_error("Unknown option: " + arg);
    }
    // Fall back to the HeraMind environment so a deployed unit needs no flags.
    const auto env_string = [](const char* name) -> std::string {
        const char* raw = std::getenv(name);
        return raw ? raw : "";
    };
    const auto env_int = [](const char* name, int& target) {
        const char* raw = std::getenv(name);
        if (!raw || !*raw) return;
        const std::string text = raw;
        size_t used = 0;
        int parsed = 0;
        try {
            parsed = std::stoi(text, &used);
        } catch (const std::exception&) {
            throw std::runtime_error(std::string(name) + " must be an integer");
        }
        if (used != text.size()) throw std::runtime_error(std::string(name) + " must be an integer");
        target = parsed;
    };
    if (o.mqtt_host.empty()) o.mqtt_host = env_string("HERAMIND_MQTT_HOST");
    if (o.mqtt_topic.empty()) o.mqtt_topic = env_string("HERAMIND_MQTT_TOPIC");
    if (o.camera_id.empty()) o.camera_id = env_string("HERAMIND_CAMERA_ID");
    if (o.run_id.empty()) o.run_id = env_string("HERAMIND_RUN_ID");
    env_int("HERAMIND_MQTT_PORT", o.mqtt_port);
    if (o.backend != "tensorrt" && o.backend != "onnx")
        throw std::runtime_error("--backend must be tensorrt or onnx");
    if (o.tracking != "bytetrack" && o.tracking != "none")
        throw std::runtime_error("--tracking must be bytetrack or none");
    if (!o.mqtt_host.empty()) {
        if (o.mqtt_port < 1 || o.mqtt_port > 65535)
            throw std::runtime_error("MQTT port must be in [1, 65535]");
        if (o.mqtt_topic.empty())
            throw std::runtime_error("--mqtt-topic (or HERAMIND_MQTT_TOPIC) is required to publish events");
        if (o.mqtt_topic.find_first_of("+#") != std::string::npos)
            throw std::runtime_error("MQTT publish topic must not contain '+' or '#'");
        if (o.camera_id.empty())
            throw std::runtime_error("--camera-id (or HERAMIND_CAMERA_ID) is required to publish events");
        // HeraMind correlates an event with its attributes and crop through the
        // tracking id, so events without a tracker would be uncorrelatable.
        if (o.tracking != "bytetrack")
            throw std::runtime_error("Event publishing needs --tracking bytetrack to identify each person");
        if (o.person_model.empty())
            throw std::runtime_error("Event publishing needs --person-model for the person branch");
    }
    const bool use_trt = o.backend == "tensorrt";
    if (o.model.empty()) o.model = use_trt
        ? "data/models/yolov11n_ppe_detection_fp16.engine"
        : "data/models/yolov11n_ppe_detection.onnx";
    if (o.output.empty()) o.output = use_trt
        ? "output/ppe/16.22.09_ppe_trt.mp4" : "output/ppe/16.22.09_ppe.mp4";
    if (!o.person_model.empty() && fs::path(o.person_model).extension() != ".engine")
        throw std::runtime_error("--person-model requires a TensorRT .engine");
    if (fs::path(o.model).extension() != (use_trt ? ".engine" : ".onnx"))
        throw std::runtime_error("Model extension does not match --backend " + o.backend);
    return o;
}

// Detach on every exit path to break the nodes' shared ownership cycles.
struct Pipeline {
    std::shared_ptr<cvedix_nodes::cvedix_app_src_node> src;
    ~Pipeline() {
        if (src) {
            src->stop();
            src->detach_recursively();
        }
    }
};
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "PPE: video -> app_src -> YOLO11 -> OSD -> app_des -> MP4/CSV\n"
                  << "Run from repository root; options:\n"
                  << "  --backend tensorrt|onnx (default: tensorrt / FP16 engine on GPU)\n"
                  << "  --video PATH --model PATH --labels PATH --output PATH.mp4\n"
                  << "  --conf 0.25 --nms 0.45 --max-frames 0 (0 = entire video)\n"
                  << "  --person-model PATH.engine --person-conf 0.35 (enable parallel person + PPE safety)\n"
                  << "  --tracking bytetrack|none (default: bytetrack on person targets)\n"
                  << "  --analysis-board 0|1 (export board PNG/MP4, default: 0)\n"
                  << "HeraMind event publishing (all required together; HERAMIND_* env fallbacks):\n"
                  << "  --mqtt-host HOST --mqtt-port 1883 --mqtt-topic TOPIC\n"
                  << "  --camera-id ID --run-id ID (camera-id becomes the HeraMind instance_id)\n"
                  << "  --confirm-frames 3 --cooldown-ms 10000 --event-archive-dir DIR\n"
                  << "Outputs silent constant-FPS video and detections CSV beside it.\n";
        return 0;
    }
    try {
        const auto o = parse(argc, argv);
        const bool use_trt = o.backend == "tensorrt";
#ifndef CVEDIX_WITH_TRT
        if (use_trt || !o.person_model.empty()) throw std::runtime_error(
            "TensorRT backend is not built. Configure with -DCVEDIX_WITH_TRT=ON, "
            "then rebuild ppe_video_sample; or select --backend onnx for CPU.");
#endif
        const auto backend = use_trt ? cvedix_nodes::BackendType::TENSORRT
                                     : cvedix_nodes::BackendType::ONNX;
        for (const auto& input : {o.video, o.model, o.labels}) {
            if (!fs::is_regular_file(input)) throw std::runtime_error("Input not found: " + input);
        }
        if (!o.person_model.empty() && !fs::is_regular_file(o.person_model))
            throw std::runtime_error("Input not found: " + o.person_model);
        fs::path output(o.output), csv_path(output);
        if (output.extension() != ".mp4") throw std::runtime_error("--output must end in .mp4");
        csv_path.replace_extension(".csv");
        const auto safety_path = output.parent_path() / (output.stem().string() + "_safety.csv");
        const auto board_path = output.parent_path() / (output.stem().string() + "_board.png");
        const auto board_video_path = output.parent_path() / (output.stem().string() + "_board.mp4");
        // Fail before opening writers, including when output aliases an input.
        if (fs::exists(output) || fs::exists(csv_path) ||
            (!o.person_model.empty() && fs::exists(safety_path)) ||
            (o.analysis_board && (fs::exists(board_path) || fs::exists(board_video_path))))
            throw std::runtime_error("Output already exists; select a new --output path: " + o.output);

        cv::setNumThreads(4);
        CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
        CVEDIX_SET_LOG_TO_FILE(false);
        CVEDIX_LOGGER_INIT();
        cv::VideoCapture capture(o.video);
        cv::Mat frame;
        if (!capture.isOpened() || !capture.read(frame))
            throw std::runtime_error("Cannot decode video: " + o.video);
        const double fps = capture.get(cv::CAP_PROP_FPS);
        if (!std::isfinite(fps) || fps <= 0) throw std::runtime_error("Invalid video FPS");
        const auto frame_size = frame.size();

        // These outlive the callbacks and all nodes, including during unwinding.
        std::mutex result_mutex;
        std::condition_variable result_ready;
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> result;
        auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
            "ppe_detector", o.model, cvedix_nodes::YoloVersion::YOLO11,
            o.labels, o.conf, o.nms, 0, backend);
        if (detector->get_active_backend_type() != backend)
            throw std::runtime_error("Detector did not activate the requested backend");
        // Person class 0 is mapped to 100, separate from PPE class 0 (helmet).
        std::shared_ptr<cvedix_nodes::cvedix_yolo_detector_node> person_detector;
        if (!o.person_model.empty()) {
            person_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
                "person_detector", o.person_model, cvedix_nodes::YoloVersion::YOLO11,
                "", o.person_conf, o.nms, 100, cvedix_nodes::BackendType::TENSORRT);
            if (person_detector->get_active_backend_type() != cvedix_nodes::BackendType::TENSORRT)
                throw std::runtime_error("Person detector did not activate TensorRT");
            person_detector->set_allowed_classes({100});
        }
        // HeraMind event publishing. The publisher is created before any node so
        // a broker that refuses the connection fails the run immediately, and it
        // is held by the event node itself so its lifetime cannot end first.
        std::shared_ptr<ppe_sample::mqtt_publisher> mqtt;
        std::shared_ptr<cvedix_nodes::cvedix_ppe_event_node> event;
        if (!o.mqtt_host.empty()) {
            mqtt = std::make_shared<ppe_sample::mqtt_publisher>(
                o.mqtt_host, o.mqtt_port, o.mqtt_topic, "cvedix-ppe-" + o.camera_id);
            cvedix_nodes::ppe_event_config event_config;
            event_config.camera_id = o.camera_id;
            event_config.run_id = o.run_id.empty() ? "cvedix-ppe" : o.run_id;
            event_config.archive_dir = o.event_archive_dir;
            event_config.source_fps = fps;
            event_config.confirm_frames = o.confirm_frames;
            event_config.cooldown_ms = o.cooldown_ms;
            event_config.format = cvedix_nodes::ppe_payload_format::heramind;
            event = std::make_shared<cvedix_nodes::cvedix_ppe_event_node>("ppe_event",
                event_config, [mqtt](const std::string& payload) { return mqtt->publish(payload); });
        }
        auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("ppe_osd");
        cvedix_nodes::unified_osd_config osd_config;
        osd_config.show_bbox = osd_config.show_label = true;
        osd_config.show_track_id = osd_config.show_track_id_in_label = false;
        osd_config.show_track_trail = osd_config.show_center_dot = false;
        osd->update_config(osd_config);
        auto sink = std::make_shared<cvedix_nodes::cvedix_app_des_node>("ppe_output", 0);
        sink->set_app_des_result_hooker([&](const std::string&, auto meta) {
            auto completed = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
            if (!completed) return;
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                result = completed;
            }
            result_ready.notify_one();
        });
        Pipeline pipeline{std::make_shared<cvedix_nodes::cvedix_app_src_node>("ppe_video", 0)};
        if (person_detector) {
            auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("ppe_split", false, true);
            auto sync = std::make_shared<cvedix_nodes::cvedix_sync_node>(
                "ppe_merge", cvedix_nodes::cvedix_sync_mode::MERGE, 60000);
            auto safety = std::make_shared<cvedix_nodes::cvedix_ba_ppe_safety_node>("ppe_safety");
            split->attach_to({pipeline.src});
            detector->attach_to({split});
            person_detector->attach_to({split});
            sync->attach_to({detector, person_detector});
            if (o.tracking == "bytetrack") {
                auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
                    "person_bytetrack", cvedix_nodes::cvedix_track_for::NORMAL,
                    o.person_conf, o.person_conf, .8f, 60,
                    static_cast<int>(std::lround(fps)), std::set<int>{100});
                tracker->attach_to({sync});
                safety->attach_to({tracker});
            } else safety->attach_to({sync});
            if (event) {
                event->attach_to({safety});
                sink->attach_to({event});
            } else sink->attach_to({safety});
        } else {
            detector->attach_to({pipeline.src});
            osd->attach_to({detector});
            sink->attach_to({osd});
        }

        if (!output.parent_path().empty()) fs::create_directories(output.parent_path());
        // Register board hooks only after the entire graph has been attached.
        // Synchronous snapshots avoid a GUI dependency and produce one board
        // frame per video frame, with the same playback duration.
        std::unique_ptr<cvedix_utils::cvedix_analysis_board> board;
        cv::VideoWriter board_writer;
        if (o.analysis_board) {
            board = std::make_unique<cvedix_utils::cvedix_analysis_board>(
                std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>{pipeline.src}, "");
            pipeline.src->invoke_stream_info_hooker(pipeline.src->node_name,
                {0, static_cast<int>(std::lround(fps)), frame.cols, frame.rows, o.video});
            const auto canvas = board->snapshot();
            board_writer.open(board_video_path.string(), cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, canvas.size());
            if (!board_writer.isOpened()) throw std::runtime_error("Cannot open board MP4 writer");
        }
        cv::VideoWriter writer(o.output, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, frame_size);
        if (!writer.isOpened()) throw std::runtime_error("Cannot open MP4 writer: " + o.output);
        std::ofstream csv(csv_path);
        csv.exceptions(std::ios::failbit | std::ios::badbit);
        csv << "frame_index,source_timestamp_ms,class_id,confidence,x,y,width,height,track_id\n";
        csv << std::fixed << std::setprecision(6);
        std::ofstream safety_csv;
        if (person_detector) {
            safety_csv.exceptions(std::ios::failbit | std::ios::badbit);
            safety_csv.open(safety_path);
            safety_csv << "frame_index,source_timestamp_ms,person_index,confidence,x,y,width,height,has_helmet,has_vest,status,track_id\n";
            safety_csv << std::fixed << std::setprecision(6);
        }
        std::signal(SIGINT, interrupt);
        std::signal(SIGTERM, interrupt);
        pipeline.src->start();

        std::cout << "Input: " << o.video << " (" << frame.cols << "x" << frame.rows
                  << ", " << fps << " FPS)\nBackend: "
                  << (use_trt ? "TensorRT / NVIDIA GPU" : "OpenCV DNN / CPU")
                  << "\nModel: " << o.model << "\nOutput: " << o.output << std::endl;
        if (person_detector) std::cout << "Person model: " << o.person_model
            << "\nTracking: " << o.tracking
            << "\nPipeline: src -> split -> [PPE || person] -> merge -> tracking -> PPE safety"
            << (event ? " -> MQTT events" : "") << " -> MP4/CSV\n" << std::flush;
        if (event) std::cout << "HeraMind events: mqtt://" << o.mqtt_host << ':' << o.mqtt_port
            << '/' << o.mqtt_topic << " as camera " << o.camera_id << std::endl;
        if (board) std::cout << "Analysis board: " << board_path << " / " << board_video_path << std::endl;
        const auto started = std::chrono::steady_clock::now();
        int processed = 0;
        size_t detections = 0;
        size_t safe_count = 0, unsafe_count = 0;
        do {
            if (interrupted || (o.max_frames && processed >= o.max_frames)) break;
            if (frame.size() != frame_size) throw std::runtime_error("Video resolution changed");
            const double timestamp_ms = capture.get(cv::CAP_PROP_POS_MSEC);
            // Preserve source FPS metadata. One in-flight frame provides backpressure:
            // offline processing never fills the queues or drops frames.
            pipeline.src->meta_flow(std::make_shared<cvedix_objects::cvedix_frame_meta>(
                frame.clone(), processed, 0, frame.cols, frame.rows, static_cast<int>(std::lround(fps))));
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> completed;
            {
                std::unique_lock<std::mutex> lock(result_mutex);
                if (!result_ready.wait_for(lock, std::chrono::seconds(60), [&] { return result != nullptr; }))
                    throw std::runtime_error("Pipeline timed out waiting for frame " + std::to_string(processed));
                completed = std::move(result);
            }
            if (completed->frame_index != processed || completed->channel_index != 0)
                throw std::runtime_error("Pipeline returned a mismatched frame");
            writer.write(completed->osd_frame.empty() ? completed->frame : completed->osd_frame);
            if (board) board_writer.write(board->snapshot());
            int person_index = 0;
            for (const auto& t : completed->targets) {
                csv << processed << ',' << timestamp_ms << ',' << t->primary_class_id << ','
                    << t->primary_score << ',' << t->x << ',' << t->y << ',' << t->width << ',' << t->height << ',' << t->track_id << '\n';
                if (person_detector && t->primary_class_id == 100) {
                    if (t->secondary_class_ids.empty() || t->secondary_labels.empty())
                        throw std::runtime_error("Missing PPE safety assessment");
                    const int mask = t->secondary_class_ids.back();
                    mask ? ++unsafe_count : ++safe_count;
                    safety_csv << processed << ',' << timestamp_ms << ',' << ++person_index << ','
                        << t->primary_score << ',' << t->x << ',' << t->y << ',' << t->width << ',' << t->height << ','
                        << ((mask & 1) == 0) << ',' << ((mask & 2) == 0) << ',' << t->secondary_labels.back() << ',' << t->track_id << '\n';
                }
            }
            detections += completed->targets.size();
            ++processed;
            if (processed % 100 == 0) std::cout << "Processed " << processed << " frames\n" << std::flush;
        } while (capture.read(frame));
        // Drain QoS 1 acknowledgements before tearing the client down, so a
        // broker that is slower than the pipeline still receives every event.
        const bool events_flushed = !mqtt || mqtt->flush();
        writer.release();
        csv.close();
        if (person_detector) safety_csv.close();
        if (board) {
            board_writer.release();
            if (!cv::imwrite(board_path.string(), board->snapshot()))
                throw std::runtime_error("Cannot save analysis board PNG");
        }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::cout << (interrupted ? "Interrupted: " : "Done: ") << processed << " frames, " << detections
                  << " detections, " << processed / elapsed << " processing FPS\nCSV: " << csv_path << std::endl;
        if (person_detector) std::cout << "PPE person observations: " << safe_count << " safe, "
            << unsafe_count << " unsafe\nSafety CSV: " << safety_path << std::endl;
        if (event) {
            const auto [sent, acknowledged] = mqtt->counts();
            std::cout << "HeraMind events: " << event->generated_count() << " generated, "
                << event->accepted_count() << " queued, " << sent << " published, "
                << acknowledged << " acknowledged, " << event->error_count() << " errors, "
                << event->skipped_untracked_count() << " skipped (no track id)" << std::endl;
            if (!events_flushed)
                std::cerr << "[ppe_video_sample] Some MQTT events were not acknowledged in time\n";
            if (event->error_count() > 0) return 2;
        }
        return interrupted ? 130 : 0;
    } catch (const std::exception& e) {
        std::cerr << "[ppe_video_sample] " << e.what() << '\n';
        return 1;
    }
}
