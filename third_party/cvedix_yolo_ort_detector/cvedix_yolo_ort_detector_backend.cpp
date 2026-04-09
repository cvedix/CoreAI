/**
 * @file cvedix_yolo_ort_detector_backend.cpp
 * @brief ORT Backend Plugin — extern "C" factory for dlopen loading
 */

#include "cvedix_yolo_ort_detector_backend.h"
#include <opencv2/opencv.hpp>

namespace cvedix_yolo_ort_backend {

bool cvedix_yolo_ort_detector_backend::detect(
    const std::vector<cv::Mat>& images,
    std::vector<std::vector<cvedix_nodes::infers::Detection>>& detections) {

    if (!detector || images.empty()) {
        return false;
    }

    try {
        std::vector<std::vector<cvedix_yolo_ort::Detection>> batch_ort_detections;
        detector->detect(images, batch_ort_detections);

        detections.resize(batch_ort_detections.size());
        for (size_t i = 0; i < batch_ort_detections.size(); i++) {
            for (const auto& det : batch_ort_detections[i]) {
                cvedix_nodes::infers::Detection result;
                result.bbox[0] = det.bbox[0];
                result.bbox[1] = det.bbox[1];
                result.bbox[2] = det.bbox[2];
                result.bbox[3] = det.bbox[3];
                result.confidence = det.conf;
                result.class_id = det.class_id;
                detections[i].push_back(result);
            }
        }
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[cvedix_yolo_ort_backend] detect() error: " << e.what() << std::endl;
        return false;
    }
}

} // namespace cvedix_yolo_ort_backend

extern "C" {

cvedix_nodes::infers::cvedix_infer_detector_backend*
create_backend(const char* model_path) {
    try {
        auto* backend = new cvedix_yolo_ort_backend::cvedix_yolo_ort_detector_backend();
        backend->init_detector(model_path);
        return backend;
    }
    catch (const std::exception& e) {
        std::cerr << "[cvedix_yolo_ort_backend] create_backend failed: " << e.what() << std::endl;
        return nullptr;
    }
}

void destroy_backend(cvedix_nodes::infers::cvedix_infer_detector_backend* backend) {
    delete backend;
}

} // extern "C"
