/**
 * @file cvedix_trt_yolov11_face_detector_node.cpp
 * @brief TensorRT YOLOv11 Face detector node implementation
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix_trt_yolov11_face_detector_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"

#ifdef CVEDIX_WITH_LICENSE
#include "cvedix/utils/license/cvedix_license_manager.h"
#endif

namespace cvedix_nodes {

cvedix_trt_yolov11_face_detector_node::cvedix_trt_yolov11_face_detector_node(
    const std::string& node_name,
    const std::string& engine_path,
    float conf_threshold,
    float nms_threshold)
    : cvedix_primary_infer_node(node_name, "", "", ""),
      conf_threshold(conf_threshold),
      nms_threshold(nms_threshold) {
    
    #ifdef CVEDIX_WITH_LICENSE
    if (!cvedix_utils::cvedix_license_manager::get_instance().check_license()) {
        throw std::runtime_error("TensorRT features require a valid license. Please contact support.");
    }
    #endif
    
    try {
        // Create TensorRT face detector
        detector = std::make_shared<trt_yolov11_face::trt_yolov11_face_detector>(
            engine_path, conf_threshold, nms_threshold);
        
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] TensorRT YOLOv11 Face Detector initialized: %dx%d, conf=%.2f, nms=%.2f, landmarks=%s",
            node_name.c_str(),
            detector->get_input_width(),
            detector->get_input_height(),
            conf_threshold,
            nms_threshold,
            detector->has_landmarks() ? "yes" : "no"));
        
        this->initialized();
    }
    catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to load TensorRT engine: %s - %s",
            node_name.c_str(), engine_path.c_str(), e.what()));
        throw;
    }
}

cvedix_trt_yolov11_face_detector_node::~cvedix_trt_yolov11_face_detector_node() {
    deinitialized();
}

void cvedix_trt_yolov11_face_detector_node::set_conf_threshold(float thresh) {
    conf_threshold = thresh;
    if (detector) {
        detector->set_conf_threshold(thresh);
    }
}

void cvedix_trt_yolov11_face_detector_node::set_nms_threshold(float thresh) {
    nms_threshold = thresh;
    if (detector) {
        detector->set_nms_threshold(thresh);
    }
}

void cvedix_trt_yolov11_face_detector_node::run_infer_combinations(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
    
    if (frame_meta_with_batch.empty()) {
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
    
    // Run TensorRT inference
    std::vector<std::vector<trt_yolov11_face::FaceDetection>> batch_detections;
    detector->detect(frames, batch_detections);
    
    auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);
    
    start_time = std::chrono::system_clock::now();
    
    // Process detections for each frame
    for (size_t b = 0; b < batch_detections.size() && b < frame_meta_with_batch.size(); b++) {
        auto& frame_meta = frame_meta_with_batch[b];
        auto& detections = batch_detections[b];
        
        for (const auto& det : detections) {
            // Convert bbox from center to corner coordinates
            cv::Rect rect = trt_yolov11_face::get_face_rect(
                frame_meta->frame, det.bbox,
                detector->get_input_width(),
                detector->get_input_height());
            
            // Skip invalid boxes
            if (rect.width <= 0 || rect.height <= 0) {
                continue;
            }
            
            // Get landmarks if available
            std::vector<std::pair<int, int>> landmarks;
            if (det.has_landmarks) {
                landmarks = trt_yolov11_face::get_landmarks(det);
            }
            
            // Create face target
            auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                rect.x, rect.y, rect.width, rect.height,
                det.conf,
                landmarks
            );
            
            frame_meta->face_targets.push_back(face_target);
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Detected %d faces in frame %d",
            node_name.c_str(), (int)detections.size(), frame_meta->frame_index));
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

void cvedix_trt_yolov11_face_detector_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs, 
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
    // Not used - postprocessing is done in run_infer_combinations
}

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_TRT
