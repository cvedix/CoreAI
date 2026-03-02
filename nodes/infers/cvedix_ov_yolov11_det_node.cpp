/**
 * @file cvedix_ov_yolov11_det_node.cpp
 * @brief OpenVINO YOLOv11 object detector implementation
 */

#ifdef CVEDIX_WITH_OPENVINO

#include "cvedix_ov_yolov11_det_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"

namespace cvedix_nodes {

cvedix_ov_yolov11_det_node::cvedix_ov_yolov11_det_node(
    const std::string& node_name,
    const std::string& model_path,
    const std::string& device,
    float conf_threshold,
    float nms_threshold)
    : cvedix_primary_infer_node(node_name, "", "", ""),
      conf_threshold(conf_threshold),
      nms_threshold(nms_threshold),
      device(device) {
    
    try {
        // Create OpenVINO detector
        detector = std::make_shared<ov_yolov11::ov_yolov11_detector>(
            model_path, device, conf_threshold, nms_threshold);
        
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] OpenVINO YOLOv11 Object Detector initialized: %dx%d, device=%s, conf=%.2f, nms=%.2f",
            node_name.c_str(),
            detector->get_input_width(),
            detector->get_input_height(),
            device.c_str(),
            conf_threshold,
            nms_threshold));
        
        this->initialized();
    }
    catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to load OpenVINO model: %s - %s",
            node_name.c_str(), model_path.c_str(), e.what()));
        throw;
    }
}

cvedix_ov_yolov11_det_node::~cvedix_ov_yolov11_det_node() {
    deinitialized();
}

void cvedix_ov_yolov11_det_node::set_conf_threshold(float thresh) {
    conf_threshold = thresh;
    if (detector) {
        detector->set_conf_threshold(thresh);
    }
}

void cvedix_ov_yolov11_det_node::set_nms_threshold(float thresh) {
    nms_threshold = thresh;
    if (detector) {
        detector->set_nms_threshold(thresh);
    }
}

void cvedix_ov_yolov11_det_node::run_infer_combinations(
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
    
    // Run OpenVINO inference
    std::vector<std::vector<ov_yolov11::Detection>> batch_detections;
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
            cv::Rect rect = ov_yolov11::get_rect(
                frame_meta->frame, det.bbox,
                detector->get_input_width(),
                detector->get_input_height());
            
            // Skip invalid boxes
            if (rect.width <= 0 || rect.height <= 0) {
                continue;
            }
            
            // Create target with "object" label
            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                rect.x, rect.y, rect.width, rect.height,
                det.class_id,
                det.conf,
                frame_meta->frame_index,
                frame_meta->channel_index,
                std::to_string(det.class_id)
            );
            
            frame_meta->targets.push_back(target);
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Detected %d objects in frame %d",
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

void cvedix_ov_yolov11_det_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs, 
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
    // Not used - postprocessing is done in run_infer_combinations
}

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_OPENVINO
