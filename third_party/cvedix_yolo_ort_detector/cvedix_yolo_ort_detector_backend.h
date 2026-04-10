/**
 * @file cvedix_yolo_ort_detector_backend.h
 * @brief ORT Backend Plugin — wraps cvedix_yolo_ort_detector to implement
 *        the cvedix_infer_detector_backend interface
 */

#pragma once

#include "../../../nodes/infers/backend/cvedix_infer_detector_backend.h"
#include "cvedix_yolo_ort_detector.h"
#include <opencv2/opencv.hpp>
#include <memory>

namespace cvedix_yolo_ort_backend {

/**
 * @brief ORT Backend — implements cvedix_infer_detector_backend using ONNX Runtime
 */
class cvedix_yolo_ort_detector_backend
    : public cvedix_nodes::infers::cvedix_infer_detector_backend {
private:
    std::unique_ptr<cvedix_yolo_ort::cvedix_yolo_ort_detector> detector;
    float conf_threshold_ = 0.45f;
    float nms_threshold_ = 0.5f;

    friend cvedix_nodes::infers::cvedix_infer_detector_backend*
    create_backend(const char* model_path);

public:
    cvedix_yolo_ort_detector_backend() = default;
    ~cvedix_yolo_ort_detector_backend() override = default;

    bool detect(
        const std::vector<cv::Mat>& images,
        std::vector<std::vector<cvedix_nodes::infers::Detection>>& detections) override;

    int get_input_width() const override {
        return detector ? detector->get_input_width() : 0;
    }

    int get_input_height() const override {
        return detector ? detector->get_input_height() : 0;
    }

    void set_conf_threshold(float threshold) override {
        conf_threshold_ = threshold;
        if (detector) detector->set_conf_threshold(threshold);
    }

    void set_nms_threshold(float threshold) override {
        nms_threshold_ = threshold;
        if (detector) detector->set_nms_threshold(threshold);
    }

    void destroy() override {
        detector.reset();
    }

    // Factory helper
    void init_detector(const char* model_path) {
        detector = std::make_unique<cvedix_yolo_ort::cvedix_yolo_ort_detector>(
            model_path, conf_threshold_, nms_threshold_);
    }
};

} // namespace cvedix_yolo_ort_backend
