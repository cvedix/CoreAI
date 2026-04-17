#pragma once

/**
 * @file onnx_yolov26_detector.h
 * @brief OpenCV DNN-based YOLOv26 detector for ONNX models
 *
 * Supports both Ultralytics YOLO26 export modes:
 * - End-to-end one-to-one head: [N, 300, 6] -> [x1, y1, x2, y2, conf, class_id]
 * - Traditional one-to-many head: [N, nc + 4, 8400] or [N, 8400, nc + 4]
 */

#include <string>
#include <vector>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>

namespace onnx_yolov26 {

struct Detection {
    float bbox[4];
    float conf;
    int class_id;
};

class onnx_yolov26_detector {
private:
    cv::dnn::Net net;

    int input_width = 640;
    int input_height = 640;
    int num_classes = 80;
    int num_boxes = 8400;

    float letterbox_scale = 1.0f;
    float letterbox_pad_x = 0.0f;
    float letterbox_pad_y = 0.0f;

    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;

    std::string output_layer_name;

    bool load_model(const std::string& onnx_path);
    void preprocess(const cv::Mat& image, cv::Mat& blob);
    void postprocess_end2end(const cv::Mat& output,
                             const cv::Size& original_size,
                             std::vector<Detection>& detections);
    void postprocess_traditional(const cv::Mat& output,
                                 const cv::Size& original_size,
                                 std::vector<Detection>& detections);
    void apply_nms(std::vector<Detection>& detections);

public:
    onnx_yolov26_detector(const std::string& onnx_path,
                          float conf_threshold = 0.25f,
                          float nms_threshold = 0.45f);

    ~onnx_yolov26_detector();

    void detect(const std::vector<cv::Mat>& images,
                std::vector<std::vector<Detection>>& detections);

    void detect(const cv::Mat& image, std::vector<Detection>& detections);

    int get_input_width() const { return input_width; }
    int get_input_height() const { return input_height; }
    int get_num_classes() const { return num_classes; }
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }

    void set_conf_threshold(float thresh) { conf_threshold = thresh; }
    void set_nms_threshold(float thresh) { nms_threshold = thresh; }
};

cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h);

} // namespace onnx_yolov26