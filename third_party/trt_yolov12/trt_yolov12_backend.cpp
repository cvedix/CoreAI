/**
 * @file trt_yolov12_backend.cpp
 * @brief TensorRT Backend Plugin Implementation
 */

#include "trt_yolov12_backend.h"

namespace trt_yolov12 {

bool trt_yolov12_backend::detect(const std::vector<cv::Mat>& images,
                              std::vector<std::vector<cvedix_nodes::infers::Detection>>& detections) {
    if (!detector || images.empty()) {
        return false;
    }

    try {
        // Run batch detection using the detector
        std::vector<std::vector<trt_yolov12::Detection>> batch_trt_detections;
        detector->detect(images, batch_trt_detections);

        // Convert results to backend format (normalized bbox)
        detections.resize(batch_trt_detections.size());
        for (size_t i = 0; i < batch_trt_detections.size(); i++) {
            for (const auto& det : batch_trt_detections[i]) {
                cvedix_nodes::infers::Detection result;
                // Coordinates should already be normalized by detector
                result.bbox[0] = det.bbox[0];     // x1
                result.bbox[1] = det.bbox[1];     // y1
                result.bbox[2] = det.bbox[2];     // x2
                result.bbox[3] = det.bbox[3];     // y2
                result.confidence = det.conf;
                result.class_id = det.class_id;
                detections[i].push_back(result);
            }
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

} // namespace trt_yolov12

extern "C" {

// Factory function: create backend and initialize with model
cvedix_nodes::infers::cvedix_infer_detector_backend* create_backend(const char* model_path) {
    try {
        auto backend = new trt_yolov12::trt_yolov12_backend();
        backend->init_detector(model_path);
        return backend;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

// Destroy function
void destroy_backend(cvedix_nodes::infers::cvedix_infer_detector_backend* backend) {
    delete backend;
}

} // extern "C"
