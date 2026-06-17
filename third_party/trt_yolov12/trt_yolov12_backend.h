#pragma once

#include "../../../nodes/infers/backend/cvedix_infer_detector_backend.h"
#include "trt_yolov12_detector.h"
#include <memory>
#include <opencv2/opencv.hpp>

namespace trt_yolov12 {

/**
 * @brief TensorRT YOLOv12 Backend Implementation
 * 
 * Wraps trt_yolov12_detector to implement the cvedix_nodes::infers::cvedix_infer_detector_backend interface
 */
class trt_yolov12_backend : public cvedix_nodes::infers::cvedix_infer_detector_backend {
private:
    std::unique_ptr<trt_yolov12_detector> detector;
    float conf_threshold = 0.45f;
    float nms_threshold = 0.5f;

    friend cvedix_nodes::infers::cvedix_infer_detector_backend* create_backend(const char* model_path);

public:
    trt_yolov12_backend() = default;

    ~trt_yolov12_backend() override = default;

    bool detect(const std::vector<cv::Mat>& images,
               std::vector<std::vector<cvedix_nodes::infers::Detection>>& detections) override;

    int get_input_width() const override {
        return detector ? detector->get_input_width() : 0;
    }

    int get_input_height() const override {
        return detector ? detector->get_input_height() : 0;
    }

    void set_conf_threshold(float threshold) override {
        conf_threshold = threshold;
        if (detector) {
            detector->set_conf_threshold(threshold);
        }
    }

    void set_nms_threshold(float threshold) override {
        nms_threshold = threshold;
        if (detector) {
            detector->set_nms_threshold(threshold);
        }
    }

    void destroy() override {
        detector.reset();
    }

    // Internal helper for factory
    void init_detector(const char* model_path) {
        detector = std::make_unique<trt_yolov12_detector>(
            model_path, conf_threshold, nms_threshold);
    }
};

} // namespace trt_yolov12
