/**
 * @file cvedix_yolo_detector_node.cpp
 * @brief Generic YOLOv11 detector implementation using plugin system
 */

#include "cvedix_yolo_detector_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace cvedix_nodes {

cvedix_yolo_detector_node::cvedix_yolo_detector_node(
    const std::string& node_name,
    const std::string& model_path,
    const std::string& labels_path,
    float conf_threshold,
    float nms_threshold,
    int class_id_offset,
    BackendType backend_type)
    : cvedix_primary_infer_node(node_name, "", "", ""),
      conf_threshold(conf_threshold),
      nms_threshold(nms_threshold),
      class_id_offset(class_id_offset) {

    // Load labels file
    if (!labels_path.empty()) {
        std::ifstream file(labels_path);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                if (!line.empty()) {
                    labels.push_back(line);
                }
            }
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Loaded %zu labels from %s",
                node_name.c_str(), labels.size(), labels_path.c_str()));
        } else {
            CVEDIX_WARN(cvedix_utils::string_format(
                "[%s] Could not load labels file: %s",
                node_name.c_str(), labels_path.c_str()));
        }
    }

    try {
        // Auto-detect hardware and determine backend type
        if (backend_type == BackendType::AUTO) {
            backend_type = detect_hw_info();
        }

        // Validate model path matches backend type
        if (!validate_model_path(backend_type, model_path)) {
            throw std::runtime_error(
                "Model path extension does not match detected backend type");
        }

        // Load backend dynamically
        backend = load_backend(backend_type, model_path);

        if (!backend) {
            throw std::runtime_error("Failed to load backend plugin: returned null");
        }

        // Set thresholds to backend (IMPORTANT: backend has its own default values)
        backend->set_conf_threshold(conf_threshold);
        backend->set_nms_threshold(nms_threshold);

        int input_w = backend->get_input_width();
        int input_h = backend->get_input_height();

        const char* backend_name = "Unknown";
        switch (backend_type) {
            case BackendType::TENSORRT:
                backend_name = "TensorRT";
                break;
            case BackendType::OPENVINO:
                backend_name = "OpenVINO";
                break;
            case BackendType::ONNX:
                backend_name = "ONNX";
                break;
            case BackendType::RKNN:
                backend_name = "RKNN";
                break;
            case BackendType::ORT:
                backend_name = "ONNX Runtime";
                break;
        }

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Backend loaded: %s, model: %s",
            node_name.c_str(), backend_name, model_path.c_str()));

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] YOLOv11 Detector initialized: %dx%d, conf=%.2f, nms=%.2f, classes=%zu, offset=%d",
            node_name.c_str(),
            input_w, input_h,
            conf_threshold, nms_threshold,
            labels.size(), class_id_offset));

        this->initialized();
    }
    catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to initialize detector: %s",
            node_name.c_str(), e.what()));
        throw;
    }
}

cvedix_yolo_detector_node::~cvedix_yolo_detector_node() {
    if (backend) {
        backend->destroy();
        backend = nullptr;
    }
    deinitialized();
}

BackendType cvedix_yolo_detector_node::detect_hw_info() const {
    // Check for RKNN (Rockchip NPU) availability
    if (std::system("ldconfig -p | grep -q librknnrt") == 0) {
        CVEDIX_INFO("[hw_info] RKNN detected, using RKNN backend");
        return BackendType::RKNN;
    }

    // Check for TensorRT availability
    // This can check for CUDA libraries, TensorRT installation, GPU driver, etc.
    if (std::system("ldconfig -p | grep -q libnvinfer") == 0) {
        CVEDIX_INFO("[hw_info] TensorRT detected, using TensorRT backend");
        return BackendType::TENSORRT;
    }

    // Check for OpenVINO availability
    if (std::system("ldconfig -p | grep -q libopenvino") == 0) {
        CVEDIX_INFO("[hw_info] OpenVINO detected, using OpenVINO backend");
        return BackendType::OPENVINO;
    }

    // Check for ONNX Runtime availability
    if (std::system("ldconfig -p | grep -q libonnxruntime") == 0) {
        CVEDIX_INFO("[hw_info] ONNX Runtime detected, using ORT backend");
        return BackendType::ORT;
    }

    // Fallback to ONNX runtime (OpenCV DNN)
    CVEDIX_INFO("[hw_info] No TensorRT/OpenVINO/RKNN/ORT found, using OpenCV DNN backend");
    return BackendType::ONNX;
}

std::string cvedix_yolo_detector_node::get_file_extension(const std::string& path) const {
    size_t pos = path.find_last_of(".");
    if (pos == std::string::npos) {
        return "";
    }
    std::string ext = path.substr(pos);
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext;
}

bool cvedix_yolo_detector_node::validate_model_path(BackendType backend_type,
                                                    const std::string& model_path) const {
    std::string ext = get_file_extension(model_path);
    
    switch (backend_type) {
        case BackendType::TENSORRT:
            if (ext != ".engine") {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[validate_model] TensorRT backend expects .engine file, got %s",
                    ext.c_str()));
                return false;
            }
            return true;

        case BackendType::OPENVINO:
            if (ext != ".xml") {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[validate_model] OpenVINO backend expects .xml file, got %s",
                    ext.c_str()));
                return false;
            }
            return true;

        case BackendType::ONNX:
            if (ext != ".onnx") {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[validate_model] ONNX backend expects .onnx file, got %s",
                    ext.c_str()));
                return false;
            }
            return true;

        case BackendType::RKNN:
            if (ext != ".rknn") {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[validate_model] RKNN backend expects .rknn file, got %s",
                    ext.c_str()));
                return false;
            }
            return true;

        case BackendType::ORT:
            if (ext != ".onnx") {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[validate_model] ORT backend expects .onnx file, got %s",
                    ext.c_str()));
                return false;
            }
            return true;

        default:
            CVEDIX_ERROR("[validate_model] Unknown backend type");
            return false;
    }
}

cvedix_nodes::infers::cvedix_infer_detector_backend* cvedix_yolo_detector_node::load_backend(
    BackendType backend_type,
    const std::string& model_path) {
    
    std::string plugin_path;
    
    switch (backend_type) {
        case BackendType::TENSORRT: {
            plugin_path = "libtrt_yolov11.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading TensorRT backend: %s", plugin_path.c_str()));
            break;
        }
        case BackendType::OPENVINO: {
            plugin_path = "libov_yolov11.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading OpenVINO backend: %s", plugin_path.c_str()));
            break;
        }
        case BackendType::ONNX: {
            plugin_path = "libonnx_yolov11.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading ONNX backend: %s", plugin_path.c_str()));
            break;
        }
        case BackendType::RKNN: {
            plugin_path = "librknn_yolov11.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading RKNN backend: %s", plugin_path.c_str()));
            break;
        }
        case BackendType::ORT: {
            plugin_path = "libcvedix_yolo_ort_detector.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading ONNX Runtime backend: %s", plugin_path.c_str()));
            break;
        }
        default:
            CVEDIX_ERROR("[load_backend] Unknown backend type");
            return nullptr;
    }

    // Load using plugin loader
    return cvedix_nodes::infers::cvedix_infer_detector_plugin_loader::load(plugin_path, model_path);
}

void cvedix_yolo_detector_node::set_conf_threshold(float thresh) {
    conf_threshold = thresh;
    if (backend) {
        backend->set_conf_threshold(thresh);
    }
}

void cvedix_yolo_detector_node::set_nms_threshold(float thresh) {
    nms_threshold = thresh;
    if (backend) {
        backend->set_nms_threshold(thresh);
    }
}

std::string cvedix_yolo_detector_node::get_label(int class_id) const {
    int idx = class_id - class_id_offset;
    if (idx >= 0 && idx < static_cast<int>(labels.size())) {
        return labels[idx];
    }
    return "class_" + std::to_string(class_id);
}

int cvedix_yolo_detector_node::get_input_width() const {
    return backend ? backend->get_input_width() : 0;
}

int cvedix_yolo_detector_node::get_input_height() const {
    return backend ? backend->get_input_height() : 0;
}

void cvedix_yolo_detector_node::run_infer_combinations(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (frame_meta_with_batch.empty() || !backend) {
        return;
    }

    auto start_time = std::chrono::system_clock::now();

    // Prepare input frames
    std::vector<cv::Mat> frames;
    for (const auto& meta : frame_meta_with_batch) {
        frames.push_back(meta->frame);
    }

    auto prepare_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);

    start_time = std::chrono::system_clock::now();

    // Run batch inference on all frames
    std::vector<std::vector<cvedix_nodes::infers::Detection>> batch_detections;
    
    bool success = backend->detect(frames, batch_detections);
    
    if (!success) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Batch inference failed for %zu frames",
            node_name.c_str(), frames.size()));
    }

    auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);

    start_time = std::chrono::system_clock::now();

    // Process detections for each frame
    for (size_t b = 0; b < batch_detections.size() && b < frame_meta_with_batch.size(); b++) {
        auto& frame_meta = frame_meta_with_batch[b];
        auto& detections = batch_detections[b];
        const auto& frame = frames[b];

        for (const auto& detection : detections) {
            // Convert bbox from center coordinates (cx, cy, w, h) to corner coordinates (x, y, w, h)
            float cx = detection.bbox[0];
            float cy = detection.bbox[1];
            float w = detection.bbox[2];
            float h = detection.bbox[3];

            int rect_x = static_cast<int>(cx - w / 2.0f);
            int rect_y = static_cast<int>(cy - h / 2.0f);
            int rect_w = static_cast<int>(w);
            int rect_h = static_cast<int>(h);

            // Clamp to frame boundaries
            rect_x = std::max(0, rect_x);
            rect_y = std::max(0, rect_y);
            rect_w = std::min(rect_w, frame.cols - rect_x);
            rect_h = std::min(rect_h, frame.rows - rect_y);

            // Skip invalid boxes
            if (rect_w <= 0 || rect_h <= 0) {
                continue;
            }

            // Map class ID
            int cid = detection.class_id + class_id_offset;

            // Skip if class not allowed
            if (!allowed_class_ids.empty() && 
                allowed_class_ids.find(cid) == allowed_class_ids.end()) {
                continue;
            }

            // Use provided label or generate
            std::string label = detection.label.empty() ? get_label(cid) : detection.label;
            
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Detection: class_id=%d, label=%s, conf=%.2f, bbox=[%d,%d,%d,%d]",
                node_name.c_str(), cid, label.c_str(), detection.confidence, rect_x, rect_y, rect_w, rect_h));

            // Create target
            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                rect_x, rect_y, rect_w, rect_h,
                cid,
                detection.confidence,
                frame_meta->frame_index,
                frame_meta->channel_index,
                label
            );

            frame_meta->targets.push_back(target);
        }

        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Detected %zu objects in frame %d",
            node_name.c_str(), detections.size(), frame_meta->frame_index));
    }

    auto postprocess_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);

    // Report timing
    cvedix_infer_node::infer_combinations_time_cost(
        frames.size(),
        prepare_time.count(),
        0,  // preprocess included in prepare
        infer_time.count(),
        postprocess_time.count());
}

void cvedix_yolo_detector_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
    // Not used - postprocessing is done in run_infer_combinations
}

}  // namespace cvedix_nodes
