/**
 * @file cvedix_trt_yolov11_plate_detector_node.h
 * @brief TensorRT YOLOv11 License Plate detector
 * 
 * High-performance license plate detection using TensorRT engine.
 * Supports YOLOv11 nano/small/medium/large/xlarge variants.
 * 
 * @section usage Usage
 * @code
 * auto detector = std::make_shared<cvedix_trt_yolov11_plate_detector_node>(
 *     "plate_detector", 
 *     "license-plate-v1n.engine",
 *     0.25f,  // confidence threshold
 *     0.45f   // NMS threshold
 * );
 * detector->attach_to({src_node});
 * @endcode
 * 
 * @section performance Expected Performance (RTX 3060 Ti)
 * - v1n.engine: ~900 FPS
 * - v1s.engine: ~600 FPS
 * - v1m.engine: ~300 FPS
 * 
 * @see cvedix_yolov11_plate_detector_node For ONNX/CPU version
 * @see cvedix_trt_yolov8_detector For general YOLOv8 detection
 */

#pragma once

#ifdef CVEDIX_WITH_TRT

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include "third_party/trt_yolov11/trt_yolov11_detector.h"

namespace cvedix_nodes {

/**
 * @brief TensorRT YOLOv11 License Plate detector node
 * 
 * Uses pre-built TensorRT engine (.engine files) for high-performance
 * license plate detection. Automatically handles preprocessing, inference,
 * and postprocessing with NMS.
 */
class cvedix_trt_yolov11_plate_detector_node : public cvedix_primary_infer_node {
private:
    std::shared_ptr<trt_yolov11::trt_yolov11_detector> detector = nullptr;
    float conf_threshold;
    float nms_threshold;
    
protected:
    /**
     * @brief Override inference pipeline for TensorRT
     * 
     * Completely replaces base class inference with TensorRT-optimized pipeline.
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
     * @param engine_path     Path to TensorRT engine file (.engine)
     * @param conf_threshold  Confidence threshold for detections (default: 0.25)
     * @param nms_threshold   NMS IoU threshold (default: 0.45)
     */
    cvedix_trt_yolov11_plate_detector_node(
        const std::string& node_name,
        const std::string& engine_path,
        float conf_threshold = 0.25f,
        float nms_threshold = 0.45f);
    
    ~cvedix_trt_yolov11_plate_detector_node();
    
    // Getters
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }
    
    // Setters
    void set_conf_threshold(float thresh);
    void set_nms_threshold(float thresh);
};

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_TRT
