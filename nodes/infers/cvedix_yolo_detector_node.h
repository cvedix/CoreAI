/**
 * @file cvedix_yolo_detector_node.h
 * @brief Generic YOLOv11 detector using plugin system (TensorRT/OpenVINO)
 */

#ifndef CVEDIX_YOLO_DETECTOR_NODE_H
#define CVEDIX_YOLO_DETECTOR_NODE_H

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include "backend/cvedix_infer_detector_backend.h"
#include "backend/cvedix_infer_detector_plugin_loader.h"

#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>

namespace cvedix_nodes {

/**
 * @enum BackendType
 * @brief Supported inference backend types
 */
enum class BackendType {
    AUTO,       ///< Auto-detect best available backend (default)
    TENSORRT,   ///< NVIDIA TensorRT backend (.engine models)
    OPENVINO,   ///< Intel OpenVINO backend (.xml models)
    ONNX,       ///< OpenCV DNN backend (.onnx models, CPU only)
    ORT,        ///< ONNX Runtime backend (.onnx models, CPU/GPU)
    RKNN        ///< Rockchip RKNN NPU backend (.rknn models)
};

/**
 * @enum YoloVersion
 * @brief Supported YOLO plugin families
 */
enum class YoloVersion {
    YOLO11,     ///< Force YOLO11 plugin family
    YOLO26      ///< Force YOLO26 plugin family
};

/**
 * @class cvedix_yolo_detector_node
 * @brief Generic YOLOv11 object detector using plugin-based backends
 * 
 * This node auto-detects hardware (RKNN, TensorRT, OpenVINO, or ONNX) and loads
 * the appropriate backend dynamically via plugins. No need to specify plugin path.
 * 
 * Example usage:
 * @code
 * auto node = std::make_shared<cvedix_yolo_detector_node>(
 *     "detector",
 *     "model.engine",   // or model.xml for OpenVINO, model.onnx for ONNX, model.rknn for RKNN
 *     "labels.txt",
 *     0.45,   // confidence threshold
 *     0.5     // NMS threshold
 * );
 * @endcode
 */
class cvedix_yolo_detector_node : public cvedix_primary_infer_node {
public:
    /**
     * @brief Constructor without model loading
     *
     * The node starts running immediately but with no active backend.
     * Call load_model() later to enable inference.
     */
    explicit cvedix_yolo_detector_node(const std::string& node_name);

    /**
     * @brief Constructor with automatic backend detection
     * Auto-detects hardware and loads appropriate backend (RKNN/TensorRT/OpenVINO/ONNX)
     * @param node_name Name of this node
     * @param model_path Path to model file (.rknn, .engine, .xml, or .onnx)
     * @param yolo_version YOLO plugin family to use
     * @param labels_path Path to labels file (optional)
     * @param conf_threshold Confidence threshold for detections
     * @param nms_threshold NMS threshold
     * @param class_id_offset Offset for class IDs (useful for remapping)
     */
    cvedix_yolo_detector_node(
        const std::string& node_name,
        const std::string& model_path,
        YoloVersion yolo_version,
        const std::string& labels_path = "",
        float conf_threshold = 0.45f,
        float nms_threshold = 0.5f,
        int class_id_offset = 0,
        BackendType backend_type = BackendType::AUTO
    );

    ~cvedix_yolo_detector_node() override;

    /**
     * @brief Load or reload a model at runtime
     *
     * If another model is active, it is unloaded first.
     * Returns false instead of throwing when the selected backend is unavailable
     * or the model/backend initialization fails.
     */
    bool load_model(
        const std::string& model_path,
        YoloVersion yolo_version,
        const std::string& labels_path = "",
        float conf_threshold = 0.45f,
        float nms_threshold = 0.5f,
        int class_id_offset = 0,
        BackendType backend_type = BackendType::AUTO);

    /**
     * @brief Unload the currently active model and backend
     */
    void unload_model();

    /**
     * @brief Get the backends currently supported by the system
     */
    std::vector<BackendType> get_supported_backends() const;

    /**
     * @brief Check whether a backend is currently supported by the system
     */
    bool is_backend_supported(BackendType backend_type) const;

    /**
     * @brief Check whether a model is currently loaded
     */
    bool has_loaded_model() const { return backend != nullptr; }

    /**
     * @brief Get the backend type currently in use
     */
    BackendType get_active_backend_type() const { return active_backend_type; }

    /**
     * @brief Set confidence threshold for filtering detections
     */
    void set_conf_threshold(float thresh);

    /**
     * @brief Set NMS threshold
     */
    void set_nms_threshold(float thresh);

    /**
     * @brief Add allowed class ID (only these classes will be kept)
     * If empty, all classes are allowed
     */
    void add_allowed_class(int class_id) {
        allowed_class_ids.insert(class_id);
    }

    /**
     * @brief Clear allowed classes (allow all)
     */
    void clear_allowed_classes() {
        allowed_class_ids.clear();
    }

    /**
     * @brief Set allowed class IDs from initializer list
     * @param class_ids Initializer list of class IDs to allow
     */
    void set_allowed_classes(const std::initializer_list<int>& class_ids) {
        allowed_class_ids.clear();
        for (int id : class_ids) {
            allowed_class_ids.insert(id);
        }
    }

    /**
     * @brief Get label for a class ID
     */
    std::string get_label(int class_id) const;

    /**
     * @brief Get input width expected by the model
     */
    int get_input_width() const;

    /**
     * @brief Get input height expected by the model
     */
    int get_input_height() const;

protected:
    /**
     * @brief Run inference on batch of frames
     * Implements the main detection pipeline
     */
    void run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    /**
     * @brief Postprocess raw outputs (not used - processing done inline)
     */
    void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

private:
    /**
     * @brief Detect available hardware and return appropriate backend type
     * @return Backend type to use (TensorRT > OpenVINO > ONNX)
     */
    BackendType detect_hw_info() const;

    /**
     * @brief Read current backend support from the host system
     */
    std::vector<BackendType> query_supported_backends() const;

    /**
     * @brief Load backend plugin based on backend type
     * @param backend_type Type of backend to load
     * @param model_path Path to model file
     * @return Pointer to backend instance, or nullptr if failed
     */
    cvedix_nodes::infers::cvedix_infer_detector_backend* load_backend(
        BackendType backend_type,
        const std::string& model_path);

    /**
     * @brief Validate model path matches expected extension for backend type
     * @param backend_type Type of backend
     * @param model_path Path to model file
     * @return true if valid
     */
    bool validate_model_path(BackendType backend_type, const std::string& model_path) const;

    /**
     * @brief Get file extension from path
     * @param path File path
     * @return File extension in lowercase (e.g., ".engine", ".xml")
     */
    std::string get_file_extension(const std::string& path) const;

    /**
     * @brief Load labels into the node label cache
     */
    bool load_labels_file(const std::string& labels_path);

    /**
     * @brief Convert backend type to human-readable text
     */
    static const char* backend_type_to_string(BackendType backend_type);

    /**
     * @brief Release the current backend/plugin resources
     */
    void unload_backend();

    // Backend interface (plugin-based)
    cvedix_nodes::infers::cvedix_infer_detector_backend* backend = nullptr;
    std::unique_ptr<cvedix_nodes::infers::cvedix_infer_detector_plugin_loader> plugin_loader;

    // Configuration
    float conf_threshold;
    float nms_threshold;
    int class_id_offset;
    YoloVersion yolo_version;
    BackendType active_backend_type = BackendType::AUTO;
    std::vector<std::string> labels;
    std::set<int> allowed_class_ids;  // Empty = allow all
};

}  // namespace cvedix_nodes

#endif  // CVEDIX_YOLO_DETECTOR_NODE_H
