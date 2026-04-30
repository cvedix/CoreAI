/**
 * @file face_detection_yolov11_trt_web_debug_sample.cpp
 * @brief YOLOv11 Face Detection + SORT Tracking + Web Debug Dashboard
 *
 * Pipeline:
 *   file_src (loop) → yolov11_face_detector → sort_tracker → osd → web_debug_des
 *
 * Features:
 *   - YOLOv11 face detection with auto backend fallback (ORT/ONNX/TRT)
 *   - SORT tracker for face tracking
 *   - OSD rendering: bounding boxes, labels, track IDs, trails
 *   - Web-based debug dashboard with OSD stream + Analysis Board
 *
 * Usage:
 *   ./face_detection_yolov11_trt_web_debug_sample [video] [model] [backend] [port]
 *
 * Open http://localhost:9091 in browser to see live dashboard.
 */

#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"

#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string first_existing(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return candidates.empty() ? "" : candidates.front();
}

cvedix_nodes::BackendType backend_from_name(const std::string& name) {
    if (name == "openvino") return cvedix_nodes::BackendType::OPENVINO;
    if (name == "tensorrt") return cvedix_nodes::BackendType::TENSORRT;
    if (name == "onnx") return cvedix_nodes::BackendType::ONNX;
    if (name == "ort") return cvedix_nodes::BackendType::ORT;
    if (name == "auto") return cvedix_nodes::BackendType::AUTO;
    throw std::runtime_error("Unsupported backend: " + name);
}

cvedix_nodes::BackendType backend_from_model_path(const std::string& model_path) {
    const auto ext = std::filesystem::path(model_path).extension().string();
    if (ext == ".xml") return cvedix_nodes::BackendType::OPENVINO;
    if (ext == ".engine") return cvedix_nodes::BackendType::TENSORRT;
    if (ext == ".onnx") return cvedix_nodes::BackendType::ONNX;
    return cvedix_nodes::BackendType::AUTO;
}

void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [video_path] [model_path] [backend] [port]\n"
        << "  video_path : input video path\n"
        << "  model_path : YOLOv11 face model (.engine TensorRT, .xml OpenVINO, .onnx)\n"
        << "  backend    : tensorrt | openvino | onnx | ort | auto\n"
        << "  port       : web dashboard port, default 9091\n\n"
        << "Pipeline:\n"
        << "  file_src → yolov11_face → sort → osd → web_debug\n\n"
        << "Example:\n"
        << "  " << program
        << " ./cvedix_data/videos/face.mp4"
        << " ./cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine tensorrt 9091\n";
}

std::string ensure_face_labels_file() {
    const std::string labels_path = "/tmp/cvedix_face_labels.txt";
    std::ofstream out(labels_path, std::ios::trunc);
    if (!out.is_open()) {
        return "";
    }
    out << "face\n";
    return labels_path;
}

}  // namespace

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    // ── Auto-detect paths ──
    std::string video_path = first_existing({
        "./cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./build/cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./cvedix_data/video/face.mp4"
    });

    // ONNX first (always available), then OpenVINO, then TRT
    std::string model_path = first_existing({
        "./cvedix_data/models/yolov11-model-face.onnx",
        "./build/cvedix_data/models/yolov11-model-face.onnx",
        "../cvedix_data/models/yolov11-model-face.onnx",
        "./cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "./build/cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "./cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine",
        "./build/cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine"
    });

    int port = 9091;
    cvedix_nodes::BackendType backend = backend_from_model_path(model_path);

    if (argc > 1) video_path = argv[1];
    if (argc > 2) model_path = argv[2];
    if (argc > 3) backend = backend_from_name(argv[3]);
    if (argc > 4) port = std::stoi(argv[4]);

    if (!std::filesystem::exists(video_path)) {
        std::cerr << "Video not found: " << video_path << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Model not found: " << model_path << std::endl;
        return 1;
    }

    CVEDIX_INFO("╔══════════════════════════════════════════════════════════════╗");
    CVEDIX_INFO("║  Face Detection YOLOv11 + SORT + Web Debug                 ║");
    CVEDIX_INFO("╚══════════════════════════════════════════════════════════════╝");
    CVEDIX_INFO("Video:   " + video_path);
    CVEDIX_INFO("Model:   " + model_path);
    CVEDIX_INFO("Backend: " + std::string(
        backend == cvedix_nodes::BackendType::TENSORRT  ? "TensorRT" :
        backend == cvedix_nodes::BackendType::OPENVINO  ? "OpenVINO" :
        backend == cvedix_nodes::BackendType::ONNX      ? "ONNX" :
        backend == cvedix_nodes::BackendType::ORT       ? "ORT" : "AUTO"));
    CVEDIX_INFO("Port:    " + std::to_string(port));

    const std::string labels_path = ensure_face_labels_file();

    // ══════════════════════════════════════
    // 1. Create pipeline nodes
    // ══════════════════════════════════════

    // Source: file with loop enabled
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6f, true);  // cycle=true

    // Detector: YOLOv11 face detection with robust backend fallback
    // Try: user-specified → ORT (ONNX Runtime) → ONNX (OpenCV DNN)
    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "face_yolov11_detector");  // deferred init

    bool detector_loaded = false;

    // Attempt 1: Use the auto-detected or user-specified backend + model
    CVEDIX_INFO("Attempt 1: " + model_path + " with selected backend");
    detector_loaded = detector->load_model(
        model_path, cvedix_nodes::YoloVersion::YOLO11, labels_path,
        0.25f, 0.45f, 0, backend);

    // Attempt 2: Try ONNX model with ORT backend (ONNX Runtime - supports YOLOv11 ops)
    if (!detector_loaded) {
        std::string onnx_model = first_existing({
            "./cvedix_data/models/yolov11-model-face.onnx",
            "./build/cvedix_data/models/yolov11-model-face.onnx",
            "../cvedix_data/models/yolov11-model-face.onnx"
        });
        if (std::filesystem::exists(onnx_model)) {
            CVEDIX_INFO("Attempt 2: " + onnx_model + " with ORT backend");
            detector_loaded = detector->load_model(
                onnx_model, cvedix_nodes::YoloVersion::YOLO11, labels_path,
                0.25f, 0.45f, 0, cvedix_nodes::BackendType::ORT);
        }
    }

    // Attempt 3: Try ONNX model with ONNX (OpenCV DNN) backend
    if (!detector_loaded) {
        std::string onnx_model = first_existing({
            "./cvedix_data/models/yolov11-model-face.onnx",
            "./build/cvedix_data/models/yolov11-model-face.onnx",
            "../cvedix_data/models/yolov11-model-face.onnx"
        });
        if (std::filesystem::exists(onnx_model)) {
            CVEDIX_INFO("Attempt 3: " + onnx_model + " with ONNX (OpenCV DNN) backend");
            detector_loaded = detector->load_model(
                onnx_model, cvedix_nodes::YoloVersion::YOLO11, labels_path,
                0.25f, 0.45f, 0, cvedix_nodes::BackendType::ONNX);
        }
    }

    if (!detector_loaded) {
        CVEDIX_ERROR("All backend attempts failed. Cannot load face model.");
        CVEDIX_ERROR("Please provide a compatible model via CLI args.");
        print_usage(argv[0]);
        return 1;
    }

    detector->set_allowed_classes({0});  // only class 0 = face

    // Tracker: SORT for face tracking
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "face_tracker",
        cvedix_nodes::cvedix_track_for::NORMAL
    );

    // OSD: rendering config
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_config;
    osd_config.show_bbox = true;
    osd_config.show_label = true;
    osd_config.show_track_id = true;
    osd_config.show_track_trail = true;
    osd_config.show_center_dot = true;
    osd_config.show_static_lines = false;
    osd_config.show_static_zones = false;
    osd_config.enable_ba_crossline = false;
    osd_config.enable_ba_crowding = false;
    osd_config.enable_ba_jam = false;
    osd_config.enable_ba_stop = false;
    osd_config.enable_ba_enter_exit = false;
    osd_config.bbox_color = cv::Scalar(241, 102, 99);     // accent indigo-ish
    osd_config.label_color = cv::Scalar(250, 139, 167);   // soft purple
    osd_config.dot_color = cv::Scalar(94, 197, 34);       // green
    osd_config.trail_color = cv::Scalar(94, 197, 34);     // green
    osd_config.label_font_scale = 0.5;
    osd->update_config(osd_config);

    // Web debug destination (jpeg_quality=50 for faster streaming)
    auto web_debug = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, port, nullptr, 50);

    // ══════════════════════════════════════
    // 2. Build pipeline
    // ══════════════════════════════════════
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    osd->attach_to({tracker});
    web_debug->attach_to({osd});

    // ══════════════════════════════════════
    // 3. Analysis Board (pipeline visualization)
    // ══════════════════════════════════════
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);  // render to buffer at 5fps (no GUI needed)
    web_debug->set_board(&board);

    // ══════════════════════════════════════
    // 4. Start pipeline
    // ══════════════════════════════════════
    file_src->start();

    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║  OmniCore Face Detection Web Debug Dashboard               ║\n"
              << "║                                                            ║\n"
              << "║  Pipeline:                                                 ║\n"
              << "║    file_src → face_yolov11 → sort → osd → web_debug     ║\n"
              << "║                                                            ║\n"
              << "║  Open in browser:                                          ║\n"
              << "║  → http://localhost:" << port << "                                    ║\n"
              << "║                                                            ║\n"
              << "║  Dashboard shows:                                          ║\n"
              << "║    • OSD stream with face detection + tracking             ║\n"
              << "║    • Analysis Board with pipeline visualization            ║\n"
              << "║    • Real-time stats (FPS, latency, objects, etc.)         ║\n"
              << "║                                                            ║\n"
              << "║  Video is looping continuously.                            ║\n"
              << "║  Press Enter to stop...                                    ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << std::endl;

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    return 0;
}
