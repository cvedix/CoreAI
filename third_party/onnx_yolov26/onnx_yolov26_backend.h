#pragma once

#include "../../../nodes/infers/backend/cvedix_infer_detector_backend.h"
#include "onnx_yolov26_detector.h"
#include <memory>
#include <opencv2/opencv.hpp>

namespace onnx_yolov26 {

/**
 * @brief OpenCV DNN ONNX YOLOv26 Backend Implementation
 * 
 * Wraps onnx_yolov26_detector to implement the cvedix_nodes::infers::cvedix_infer_detector_backend interface
 */
class onnx_yolov26_backend : public cvedix_nodes::infers::cvedix_infer_detector_backend {
private:
    std::unique_ptr<onnx_yolov26_detector> detector;
    float conf_threshold = 0.45f;
    float nms_threshold = 0.5f;

    friend cvedix_nodes::infers::cvedix_infer_detector_backend* create_backend(const char* model_path);

public:
    onnx_yolov26_backend() = default;

    ~onnx_yolov26_backend() override = default;

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
        detector = std::make_unique<onnx_yolov26_detector>(
            model_path, conf_threshold, nms_threshold);
    }
};

} // namespace onnx_yolov26
