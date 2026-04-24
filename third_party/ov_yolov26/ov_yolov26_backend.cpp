/**
 * @file ov_yolov11_backend.cpp
 * @brief OpenVINO Backend Plugin Implementation
 */

#ifdef CVEDIX_WITH_OPENVINO

#include "ov_yolov26_backend.h"

namespace ov_yolov26 {

bool ov_yolov26_backend::detect(const std::vector<cv::Mat>& images,
                              std::vector<std::vector<cvedix_nodes::infers::Detection>>& detections) {
    if (!detector || images.empty()) {
        return false;
    }

    try {
        // Run batch detection using the detector
        std::vector<std::vector<ov_yolov26::Detection>> batch_ov_detections;
        detector->detect(images, batch_ov_detections);

        // Convert results to backend format (normalized bbox)
        detections.resize(batch_ov_detections.size());
        for (size_t i = 0; i < batch_ov_detections.size(); i++) {
            for (const auto& det : batch_ov_detections[i]) {
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

} // namespace ov_yolov26

extern "C" {

// Factory function: create backend and initialize with model
cvedix_nodes::infers::cvedix_infer_detector_backend* create_backend(const char* model_path) {
    try {
        auto backend = new ov_yolov26::ov_yolov26_backend();
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

#endif // CVEDIX_WITH_OPENVINO
