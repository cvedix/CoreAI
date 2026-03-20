#pragma once

/**
 * @file onnx_yolov11_detector.h
 * @brief OpenCV DNN-based YOLOv11 detector for ONNX models (CPU)
 *
 * Uses OpenCV's cv::dnn module to load and run ONNX YOLOv11 models on CPU.
 * Supports single output format: [1, num_detections, 84] (4 bbox + 80 classes)
 */

#include <string>
#include <vector>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>

namespace onnx_yolov11 {

// Detection result structure
struct Detection {
    float bbox[4];    // center_x, center_y, width, height (scaled to original image)
    float conf;       // confidence score
    int class_id;     // class ID
};

/**
 * @brief OpenCV DNN-based YOLOv11 detector class (CPU)
 *
 * Loads ONNX models using cv::dnn and performs inference on CPU.
 * Supports batch processing and NMS filtering.
 */
class onnx_yolov11_detector {
private:
    // OpenCV DNN network
    cv::dnn::Net net;

    // Model info
    int input_width = 640;
    int input_height = 640;
    int num_classes = 80;
    int num_boxes = 8400;        // typical for YOLOv11

    // Detection parameters
    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;

    // Output layer name for single-head models
    std::string output_layer_name;

    // Internal methods
    bool load_model(const std::string& onnx_path);
    void preprocess(const cv::Mat& image, cv::Mat& blob);
    void postprocess(cv::Mat& output, const cv::Size& original_size, std::vector<Detection>& detections);
    void apply_nms(std::vector<Detection>& detections);

    // Sigmoid function
    static float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

public:
    onnx_yolov11_detector(const std::string& onnx_path,
                          float conf_threshold = 0.25f,
                          float nms_threshold = 0.45f);

    ~onnx_yolov11_detector();

    void detect(const std::vector<cv::Mat>& images,
                std::vector<std::vector<Detection>>& detections);

    void detect(const cv::Mat& image, std::vector<Detection>& detections);

    // Getters
    int get_input_width() const { return input_width; }
    int get_input_height() const { return input_height; }
    int get_num_classes() const { return num_classes; }
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }

    // Setters
    void set_conf_threshold(float thresh) { conf_threshold = thresh; }
    void set_nms_threshold(float thresh) { nms_threshold = thresh; }
};

/**
 * @brief Convert bbox from center coordinates to corner coordinates
 */
cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h);

} // namespace onnx_yolov11
