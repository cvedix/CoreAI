#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
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
        << "  model_path : YOLOv11 face model (.xml OpenVINO, .engine TensorRT, .onnx OpenCV DNN)\n"
        << "  backend    : openvino | tensorrt | onnx | ort | auto\n"
        << "  port       : web dashboard port, default 9091\n\n"
        << "Example:\n"
        << "  source ~/intel/openvino_2025.4.0/setupvars.sh\n"
        << "  " << program
        << " ./build/cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4"
        << " ./build/cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml openvino 9091\n";
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

    std::string video_path = first_existing({
        "./build/cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./cvedix_data/video/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4"
    });

    std::string model_path = first_existing({
        "./build/cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "../cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "./cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "./build/cvedix_data/models/yolov11-model-face.onnx",
        "../cvedix_data/models/yolov11-model-face.onnx",
        "./cvedix_data/models/yolov11-model-face.onnx"
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

    CVEDIX_INFO("===== YOLOv11 Face Web Debug Sample =====");
    CVEDIX_INFO("Video: " + video_path);
    CVEDIX_INFO("Model: " + model_path);
    CVEDIX_INFO("Port:  " + std::to_string(port));

    const std::string labels_path = ensure_face_labels_file();

    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6f, true);

    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "face_yolov11_detector",
        model_path,
        cvedix_nodes::YoloVersion::YOLO11,
        labels_path,
        0.25f,
        0.45f,
        0,
        backend);
    detector->set_allowed_classes({0});

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_config;
    osd_config.show_bbox = true;
    osd_config.show_label = true;
    osd_config.show_track_id = false;
    osd_config.show_track_trail = false;
    osd_config.show_center_dot = false;
    osd_config.show_static_lines = false;
    osd_config.show_static_zones = false;
    osd_config.bbox_color = cv::Scalar(255, 255, 255);
    osd_config.label_color = cv::Scalar(255, 255, 255);
    osd_config.dot_color = cv::Scalar(255, 255, 255);
    osd_config.trail_color = cv::Scalar(255, 255, 255);
    osd->update_config(osd_config);

    auto web_debug = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, port, nullptr, 80);

    detector->attach_to({file_src});
    osd->attach_to({detector});
    web_debug->attach_to({osd});

    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);
    web_debug->set_board(&board);

    file_src->start();

    std::cout << "\n"
              << "Open web debug dashboard: http://localhost:" << port << "\n"
              << "Press Enter to stop...\n"
              << std::endl;

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    return 0;
}
