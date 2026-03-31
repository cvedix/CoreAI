#pragma once

#include "../../../nodes/infers/backend/cvedix_infer_detector_backend.h"
#include "rknn_yolov11.h"
#include <memory>
#include <opencv2/opencv.hpp>
#include <iostream>

namespace rknn_yolov11 {

/**
 * @brief RKNN YOLOv11 Backend Implementation
 * 
 * Wraps rknn_yolov11_detector to implement the cvedix_nodes::infers::cvedix_infer_detector_backend interface
 */
class rknn_yolov11_backend : public cvedix_nodes::infers::cvedix_infer_detector_backend {
private:
    std::unique_ptr<rknn_yolov11_detector> detector;
    float conf_threshold = 0.45f;
    float nms_threshold = 0.5f;
    int num_classes = 80;

    friend cvedix_nodes::infers::cvedix_infer_detector_backend* create_backend(const char* model_path);

public:
    rknn_yolov11_backend() = default;

    ~rknn_yolov11_backend() override = default;

    bool detect(const std::vector<cv::Mat>& images,
               std::vector<std::vector<cvedix_nodes::infers::Detection>>& detections) override;

    int get_input_width() const override {
        return 640;  // RKNN YOLOv11 fixed at 640x640
    }

    int get_input_height() const override {
        return 640;
    }

    void set_conf_threshold(float threshold) override {
        conf_threshold = threshold;
    }

    void set_nms_threshold(float threshold) override {
        nms_threshold = threshold;
    }

    void destroy() override {
        detector.reset();
    }

    // Internal helper for factory
    void init_detector(const char* model_path, int num_classes) {
        // std::cout << "[RKNN Backend] init_detector() called with model: " << model_path 
        //           << ", num_classes: " << num_classes << std::endl;
        
        this->num_classes = num_classes;
        detector = std::make_unique<rknn_yolov11_detector>(model_path, num_classes);
        
        // std::cout << "[RKNN Backend] Calling detector->init()" << std::endl;
        int ret = detector->init();
        if (ret != 0) {
            std::cout << "[RKNN Backend] ERROR: detector->init() failed with code " << ret << std::endl;
            throw std::runtime_error("Failed to initialize RKNN detector");
        }
        
        // std::cout << "[RKNN Backend] init_detector() completed successfully" << std::endl;
    }
};

} // namespace rknn_yolov11
