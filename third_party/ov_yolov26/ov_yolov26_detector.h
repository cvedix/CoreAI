#pragma once

/**
 * @file ov_yolov26_detector.h
 * @brief OpenVINO YOLOv26 detector with end-to-end and one-to-many output support
 */

#ifdef CVEDIX_WITH_OPENVINO

#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <openvino/openvino.hpp>

namespace ov_yolov26 {

struct Detection {
    float bbox[4];
    float conf;
    int class_id;
    std::string class_name;
};

class ov_yolov26_detector {
private:
    ov::Core core;
    std::shared_ptr<ov::Model> model;
    ov::CompiledModel compiled_model;
    ov::InferRequest infer_request;

    int input_width = 640;
    int input_height = 640;
    int num_classes = 80;
    int num_boxes = 8400;
    int output_rows = 0;
    int output_cols = 0;
    bool transpose_output = false;
    bool end2end_output = false;

    float letterbox_scale = 1.0f;
    float letterbox_pad_x = 0.0f;
    float letterbox_pad_y = 0.0f;

    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;

    std::vector<std::string> labels;

    bool load_model(const std::string& model_path, const std::string& device);
    bool load_labels(const std::string& labels_path);
    bool load_metadata_yaml(const std::string& yaml_path);
    void preprocess(const cv::Mat& image, float* input_buffer);
    float output_at(const float* output, int row, int col) const;
    void postprocess(const float* output,
                     std::vector<Detection>& detections,
                     const cv::Size& original_size);
    void apply_nms(std::vector<Detection>& detections);

public:
    ov_yolov26_detector(const std::string& model_path,
                       const std::string& device = "CPU",
                       float conf_threshold = 0.25f,
                       float nms_threshold = 0.45f,
                       const std::string& labels_path = "");

    ~ov_yolov26_detector();

    void detect(const std::vector<cv::Mat>& images,
                std::vector<std::vector<Detection>>& detections);

    void detect(const cv::Mat& image, std::vector<Detection>& detections);

    int get_input_width() const { return input_width; }
    int get_input_height() const { return input_height; }
    int get_num_classes() const { return num_classes; }
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }
    const std::vector<std::string>& get_labels() const { return labels; }

    void set_conf_threshold(float thresh) { conf_threshold = thresh; }
    void set_nms_threshold(float thresh) { nms_threshold = thresh; }
    void set_labels(const std::vector<std::string>& new_labels) { labels = new_labels; }
};

cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h);

} // namespace ov_yolov26

#endif // CVEDIX_WITH_OPENVINO