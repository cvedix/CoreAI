/**
 * @file cvedix_trt_yolov11_det_node.h
 * @brief Generic TensorRT YOLOv11 object detector
 *
 * General-purpose object detection using TensorRT engine.
 * Supports any YOLOv11 model (COCO, custom classes, plates, etc.)
 *
 * @section usage Usage
 * @code
 * // General object detection
 * auto detector = std::make_shared<cvedix_trt_yolov11_det_node>(
 *     "detector",
 *     "yolov11n.engine",
 *     "coco_80_labels_list.txt",
 *     0.25f, 0.45f
 * );
 *
 * // Plate detection (use labels file with "plate")
 * auto plate_det = std::make_shared<cvedix_trt_yolov11_det_node>(
 *     "plate_detector",
 *     "license-plate-v1n.engine",
 *     "plate_labels.txt",
 *     0.25f, 0.45f
 * );
 * @endcode
 *
 * @see cvedix_trt_yolov11_face_detector_node For face-specific variant
 * @see cvedix_yolov11_detector_node For ONNX/CPU version
 */

#pragma once

#ifdef CVEDIX_WITH_TRT

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include "third_party/trt_yolov11/trt_yolov11_detector.h"
#include <fstream>
#include <set>

namespace cvedix_nodes {

/**
 * @brief Generic TensorRT YOLOv11 object detector node
 *
 * Uses pre-built TensorRT engine (.engine) for high-performance
 * object detection. Supports labels file for class names.
 * Replaces both cvedix_trt_yolov11_detector_node and
 * cvedix_trt_yolov11_plate_detector_node.
 */
class cvedix_trt_yolov11_det_node : public cvedix_primary_infer_node {
private:
    std::shared_ptr<trt_yolov11::trt_yolov11_detector> detector = nullptr;
    float conf_threshold;
    float nms_threshold;

    /// @brief Class labels loaded from file
    std::vector<std::string> labels;

    /// @brief Class ID offset for multi-detector pipelines
    int class_id_offset = 0;

    /// @brief Allowed class IDs (empty = all classes)
    std::set<int> allowed_class_ids;

protected:
    virtual void run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    virtual void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

public:
    /**
     * @brief Constructor
     *
     * @param node_name Unique name for this node
     * @param engine_path Path to TensorRT engine file (.engine)
     * @param labels_path Path to labels file (one label per line), empty = no labels
     * @param conf_threshold Confidence threshold (default: 0.25)
     * @param nms_threshold NMS IoU threshold (default: 0.45)
     * @param class_id_offset Offset added to class IDs (for multi-detector pipelines)
     */
    cvedix_trt_yolov11_det_node(
        const std::string& node_name,
        const std::string& engine_path,
        const std::string& labels_path = "",
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f,
        int class_id_offset = 0);

    ~cvedix_trt_yolov11_det_node();

    // Getters
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }
    const std::vector<std::string>& get_labels() const { return labels; }

    // Setters
    void set_conf_threshold(float thresh);
    void set_nms_threshold(float thresh);

    std::string get_label(int class_id) const;

    /**
     * @brief Filter detections to only specified class IDs
     * @param class_ids Set of allowed class IDs (empty = all)
     */
    void set_allowed_classes(const std::set<int>& class_ids) { allowed_class_ids = class_ids; }
};

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_TRT
