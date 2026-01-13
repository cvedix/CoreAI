/**
 * @file cvedix_trt_yolov11_face_detector_node.h
 * @brief TensorRT YOLOv11 Face detector node
 * 
 * High-performance face detection using TensorRT engine.
 * Supports YOLOv11 nano/small/medium/large/xlarge variants trained for face detection.
 * 
 * @section usage Usage
 * @code
 * auto detector = std::make_shared<cvedix_trt_yolov11_face_detector_node>(
 *     "face_detector", 
 *     "yolov11_face_fp16.engine",
 *     0.5f,   // confidence threshold
 *     0.45f   // NMS threshold
 * );
 * detector->attach_to({src_node});
 * @endcode
 * 
 * @section output Output
 * Detections are added to `frame_meta->face_targets` as `cvedix_frame_face_target` objects,
 * which include bounding box, confidence, and 5-point landmarks (if model supports).
 * 
 * @section performance Expected Performance (RTX 3060 Ti)
 * - YOLOv11n-face: ~700+ FPS
 * - YOLOv11s-face: ~500+ FPS
 * - YOLOv11m-face: ~300+ FPS
 * 
 * @see cvedix_yunet_face_detector_node For CPU-based face detection
 * @see cvedix_trt_yolov11_plate_detector_node For license plate detection
 */

#pragma once

#ifdef CVEDIX_WITH_TRT

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"
#include "third_party/trt_yolov11_face/trt_yolov11_face_detector.h"

namespace cvedix_nodes {

/**
 * @brief TensorRT YOLOv11 Face detector node
 * 
 * Uses pre-built TensorRT engine (.engine files) for high-performance
 * face detection. Automatically handles preprocessing, inference,
 * and postprocessing with NMS. Outputs face targets with optional landmarks.
 */
class cvedix_trt_yolov11_face_detector_node : public cvedix_primary_infer_node {
private:
    std::shared_ptr<trt_yolov11_face::trt_yolov11_face_detector> detector = nullptr;
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
     * @param conf_threshold  Confidence threshold for detections (default: 0.5)
     * @param nms_threshold   NMS IoU threshold (default: 0.45)
     */
    cvedix_trt_yolov11_face_detector_node(
        const std::string& node_name,
        const std::string& engine_path,
        float conf_threshold = 0.5f,
        float nms_threshold = 0.45f);
    
    ~cvedix_trt_yolov11_face_detector_node();
    
    // Getters
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }
    bool has_landmarks() const { return detector ? detector->has_landmarks() : false; }
    
    // Setters
    void set_conf_threshold(float thresh);
    void set_nms_threshold(float thresh);
};

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_TRT
