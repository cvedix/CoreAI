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
#include <filesystem>
#include <sstream>

namespace cvedix_nodes {
namespace {

bool has_library_in_directory(const std::string& dir, const std::vector<std::string>& library_names) {
    if (dir.empty()) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return false;
    }

    for (const auto& name : library_names) {
        if (std::filesystem::exists(std::filesystem::path(dir) / name, ec)) {
            return true;
        }
    }

    return false;
}

bool has_library_in_colon_paths(const char* paths, const std::vector<std::string>& library_names) {
    if (!paths) {
        return false;
    }

    std::stringstream ss(paths);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (has_library_in_directory(dir, library_names)) {
            return true;
        }
    }

    return false;
}

bool has_openvino_runtime_from_environment() {
    if (has_library_in_colon_paths(std::getenv("LD_LIBRARY_PATH"), {"libopenvino.so"})) {
        return true;
    }

    const char* openvino_root = std::getenv("INTEL_OPENVINO_DIR");
    if (!openvino_root) {
        openvino_root = std::getenv("OPENVINO_ROOT");
    }

    if (!openvino_root) {
        return false;
    }

    return has_library_in_directory(std::string(openvino_root) + "/runtime/lib/intel64",
                                    {"libopenvino.so"});
}

bool has_onnxruntime_from_environment() {
    return has_library_in_colon_paths(std::getenv("LD_LIBRARY_PATH"), {"libonnxruntime.so"});
}

}  // namespace

cvedix_yolo_detector_node::cvedix_yolo_detector_node(const std::string& node_name)
    : cvedix_primary_infer_node(node_name, "", "", ""),
      conf_threshold(0.45f),
      nms_threshold(0.5f),
      class_id_offset(0),
    yolo_version(YoloVersion::YOLO11) {
    const auto supported_backends = get_supported_backends();
    std::string supported_list;
    for (size_t i = 0; i < supported_backends.size(); ++i) {
        if (i > 0) {
            supported_list += ", ";
        }
        supported_list += backend_type_to_string(supported_backends[i]);
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Supported backends: %s",
        node_name.c_str(),
        supported_list.empty() ? "None" : supported_list.c_str()));

    this->initialized();
}

cvedix_yolo_detector_node::cvedix_yolo_detector_node(
    const std::string& node_name,
    const std::string& model_path,
    YoloVersion yolo_version,
    const std::string& labels_path,
    float conf_threshold,
    float nms_threshold,
    int class_id_offset,
    BackendType backend_type)
    : cvedix_yolo_detector_node(node_name) {
    try {
        if (!load_model(model_path,
                        yolo_version,
                        labels_path,
                        conf_threshold,
                        nms_threshold,
                        class_id_offset,
                        backend_type)) {
            throw std::runtime_error("Failed to load model/backend at construction time");
        }
    }
    catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to initialize detector: %s",
            node_name.c_str(), e.what()));
        throw;
    }
}

cvedix_yolo_detector_node::~cvedix_yolo_detector_node() {
    unload_model();
    deinitialized();
}

bool cvedix_yolo_detector_node::load_model(
    const std::string& model_path,
    YoloVersion yolo_version,
    const std::string& labels_path,
    float conf_threshold,
    float nms_threshold,
    int class_id_offset,
    BackendType backend_type) {

    unload_model();

    this->model_path = model_path;
    this->labels_path = labels_path;
    this->conf_threshold = conf_threshold;
    this->nms_threshold = nms_threshold;
    this->class_id_offset = class_id_offset;
    this->yolo_version = yolo_version;

    if (model_path.empty()) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Empty model path, skip loading",
            node_name.c_str()));
        return false;
    }

    if (!labels_path.empty()) {
        load_labels_file(labels_path);
    }

    BackendType selected_backend = backend_type;
    if (selected_backend == BackendType::AUTO) {
        selected_backend = detect_hw_info();
    }

    if (!is_backend_supported(selected_backend)) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Backend %s is not supported on this system",
            node_name.c_str(), backend_type_to_string(selected_backend)));
        unload_model();
        return false;
    }

    if (!validate_model_path(selected_backend, model_path)) {
        unload_model();
        return false;
    }

    backend = load_backend(selected_backend, model_path);
    if (!backend) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to load backend plugin for model %s",
            node_name.c_str(), model_path.c_str()));
        unload_model();
        return false;
    }

    backend->set_conf_threshold(conf_threshold);
    backend->set_nms_threshold(nms_threshold);

    active_backend_type = selected_backend;
    input_width = backend->get_input_width();
    input_height = backend->get_input_height();

    const char* model_family_name =
        yolo_version == YoloVersion::YOLO26 ? "YOLOv26" : "YOLOv11";

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Backend loaded: %s, model: %s",
        node_name.c_str(), backend_type_to_string(selected_backend), model_path.c_str()));

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] %s Detector loaded at runtime: %dx%d, conf=%.2f, nms=%.2f, classes=%zu, offset=%d",
        node_name.c_str(),
        model_family_name,
        input_width, input_height,
        conf_threshold, nms_threshold,
        labels.size(), class_id_offset));

    return true;
}

void cvedix_yolo_detector_node::unload_model() {
    unload_backend();
    labels.clear();
    model_path.clear();
    model_config_path.clear();
    labels_path.clear();
    active_backend_type = BackendType::AUTO;
    input_width = 0;
    input_height = 0;
}

std::vector<BackendType> cvedix_yolo_detector_node::get_supported_backends() const {
    return query_supported_backends();
}

bool cvedix_yolo_detector_node::is_backend_supported(BackendType backend_type) const {
    if (backend_type == BackendType::AUTO) {
        return true;
    }

    const auto supported_backends = query_supported_backends();
    return std::find(supported_backends.begin(), supported_backends.end(), backend_type) != supported_backends.end();
}

BackendType cvedix_yolo_detector_node::detect_hw_info() const {
    const auto supported_backends = query_supported_backends();
    if (std::find(supported_backends.begin(), supported_backends.end(), BackendType::RKNN) != supported_backends.end()) {
        CVEDIX_INFO("[hw_info] RKNN detected, using RKNN backend");
        return BackendType::RKNN;
    }

    if (std::find(supported_backends.begin(), supported_backends.end(), BackendType::TENSORRT) != supported_backends.end()) {
        CVEDIX_INFO("[hw_info] TensorRT detected, using TensorRT backend");
        return BackendType::TENSORRT;
    }

    if (std::find(supported_backends.begin(), supported_backends.end(), BackendType::OPENVINO) != supported_backends.end()) {
        CVEDIX_INFO("[hw_info] OpenVINO detected, using OpenVINO backend");
        return BackendType::OPENVINO;
    }

    if (std::find(supported_backends.begin(), supported_backends.end(), BackendType::ORT) != supported_backends.end()) {
        CVEDIX_INFO("[hw_info] ONNX Runtime detected, using ORT backend");
        return BackendType::ORT;
    }

    // Fallback to ONNX runtime (OpenCV DNN)
    CVEDIX_INFO("[hw_info] No TensorRT/OpenVINO/RKNN/ORT found, using OpenCV DNN backend");
    return BackendType::ONNX;
}

std::vector<BackendType> cvedix_yolo_detector_node::query_supported_backends() const {
    std::vector<BackendType> supported_backends;

    if (std::system("ldconfig -p | grep -q librknnrt") == 0) {
        supported_backends.push_back(BackendType::RKNN);
    }
    if (std::system("ldconfig -p | grep -q libnvinfer") == 0) {
        supported_backends.push_back(BackendType::TENSORRT);
    }
    if (std::system("ldconfig -p | grep -q libopenvino") == 0 || has_openvino_runtime_from_environment()) {
        supported_backends.push_back(BackendType::OPENVINO);
    }
    if (std::system("ldconfig -p | grep -q libonnxruntime") == 0 || has_onnxruntime_from_environment()) {
        supported_backends.push_back(BackendType::ORT);
    }

    supported_backends.push_back(BackendType::ONNX);
    return supported_backends;
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

bool cvedix_yolo_detector_node::load_labels_file(const std::string& labels_path) {
    labels.clear();

    std::ifstream file(labels_path);
    if (!file.is_open()) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Could not load labels file: %s",
            node_name.c_str(), labels_path.c_str()));
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            labels.push_back(line);
        }
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Loaded %zu labels from %s",
        node_name.c_str(), labels.size(), labels_path.c_str()));
    return true;
}

const char* cvedix_yolo_detector_node::backend_type_to_string(BackendType backend_type) {
    switch (backend_type) {
        case BackendType::TENSORRT:
            return "TensorRT";
        case BackendType::OPENVINO:
            return "OpenVINO";
        case BackendType::ONNX:
            return "ONNX";
        case BackendType::RKNN:
            return "RKNN";
        case BackendType::ORT:
            return "ONNX Runtime";
        case BackendType::AUTO:
        default:
            return "AUTO";
    }
}

void cvedix_yolo_detector_node::unload_backend() {
    if (plugin_loader) {
        plugin_loader->unload(backend);
        plugin_loader.reset();
        return;
    }

    if (backend) {
        backend->destroy();
        delete backend;
        backend = nullptr;
    }
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
    bool use_yolo26_plugin = yolo_version == YoloVersion::YOLO26;
    
    switch (backend_type) {
        case BackendType::TENSORRT: {
            plugin_path = use_yolo26_plugin ? "libtrt_yolov26.so" : "libtrt_yolov11.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading TensorRT backend: %s", plugin_path.c_str()));
            break;
        }
        case BackendType::OPENVINO: {
            plugin_path = use_yolo26_plugin ? "libov_yolov26.so" : "libov_yolov11.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading OpenVINO backend: %s", plugin_path.c_str()));
            break;
        }
        case BackendType::ONNX: {
            plugin_path = use_yolo26_plugin ? "libonnx_yolov26.so" : "libonnx_yolov11.so";
            CVEDIX_INFO(cvedix_utils::string_format(
                "[load_backend] Loading ONNX backend: %s", plugin_path.c_str()));
            break;
        }
        case BackendType::RKNN: {
            plugin_path = use_yolo26_plugin ? "librknn_yolov26.so" : "librknn_yolov11.so";
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

    plugin_loader = std::make_unique<cvedix_nodes::infers::cvedix_infer_detector_plugin_loader>();
    cvedix_nodes::infers::cvedix_infer_detector_backend* loaded_backend = nullptr;
    if (!plugin_loader->load(plugin_path, model_path, loaded_backend)) {
        plugin_loader.reset();
        return nullptr;
    }

    return loaded_backend;
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
