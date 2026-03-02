/**
 * @file cvedix_ov_yolov11_det_node.h
 * @brief OpenVINO YOLOv11 object detector
 * 
 * High-performance object detection using OpenVINO.
 * Supports YOLOv11 nano/small/medium/large/xlarge variants.
 * 
 * @section usage Usage
 * @code
 * auto detector = std::make_shared<cvedix_ov_yolov11_det_node>(
 *     "object_detector", 
 *     "object-detection-v1n.xml",
 *     "CPU",  // or "GPU", "AUTO"
 *     0.25f,  // confidence threshold
 *     0.45f   // NMS threshold
 * );
 * detector->attach_to({src_node});
 * @endcode
 * 
 * @section performance Expected Performance
 * - CPU (Intel Core i7): ~200-400 FPS (nano model)
 * - GPU (Intel iGPU): ~500-800 FPS (nano model)
 * 
 * @see cvedix_trt_yolov11_plate_detector_node For TensorRT/CUDA version
 * @see cvedix_yolov11_plate_detector_node For ONNX/CPU version
 */

#pragma once

#ifdef CVEDIX_WITH_OPENVINO

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include "third_party/ov_yolov11/ov_yolov11_detector.h"

namespace cvedix_nodes {

/**
 * @brief OpenVINO YOLOv11 object detector node
 * 
 * Uses OpenVINO IR (.xml/.bin) or ONNX models for high-performance
 * object detection. Automatically handles preprocessing, inference,
 * and postprocessing with NMS.
 */
class cvedix_ov_yolov11_det_node : public cvedix_primary_infer_node {
private:
    std::shared_ptr<ov_yolov11::ov_yolov11_detector> detector = nullptr;
    float conf_threshold;
    float nms_threshold;
    std::string device;
    
protected:
    /**
     * @brief Override inference pipeline for OpenVINO
     * 
     * Completely replaces base class inference with OpenVINO-optimized pipeline.
     */
    virtual void run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    
    /**
     * @brief Postprocess stub (not used, postprocessing is in run_infer_combinations)
     */
    virtual void postprocess(
        const std::vector<cv::Mat>& raw_outputs, 
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

public:
    /**
     * @brief Constructor
     * 
     * @param node_name       Unique name for this node
     * @param model_path      Path to OpenVINO model file (.xml or .onnx)
     * @param device          Device to run on ("CPU", "GPU", "AUTO", etc.)
     * @param conf_threshold  Confidence threshold for detections (default: 0.25)
     * @param nms_threshold   NMS IoU threshold (default: 0.45)
     */
    cvedix_ov_yolov11_det_node(
        const std::string& node_name,
        const std::string& model_path,
        const std::string& device = "CPU",
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f);
    
    ~cvedix_ov_yolov11_det_node();
    
    // Getters
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }
    std::string get_device() const { return device; }
    
    // Setters
    void set_conf_threshold(float thresh);
    void set_nms_threshold(float thresh);
};

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_OPENVINO
