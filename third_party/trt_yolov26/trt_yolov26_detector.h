#pragma once

/**
 * @file trt_yolov26_detector.h
 * @brief TensorRT YOLOv26 detector with end-to-end and traditional output support
 */

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <cuda_runtime_api.h>

namespace trt_yolov26 {

struct Detection {
    float bbox[4];
    float conf;
    int class_id;
};

class trt_yolov26_detector {
private:
    nvinfer1::IRuntime* runtime = nullptr;
    nvinfer1::ICudaEngine* engine = nullptr;
    nvinfer1::IExecutionContext* context = nullptr;

    cudaStream_t stream{};
    std::vector<void*> device_buffers;
    std::vector<size_t> tensor_sizes;
    float* host_output = nullptr;

    int num_io_tensors = 0;
    int input_tensor_idx = -1;
    int output_tensor_idx = -1;
    int output_rows = 0;
    int output_cols = 0;
    bool transpose_output = false;
    bool end2end_output = false;

    int input_width = 640;
    int input_height = 640;
    int num_classes = 80;
    int num_boxes = 8400;

    float letterbox_scale = 1.0f;
    float letterbox_pad_x = 0.0f;
    float letterbox_pad_y = 0.0f;

    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;

    class Logger : public nvinfer1::ILogger {
    public:
        void log(Severity severity, const char* msg) noexcept override;
    } logger;

    bool load_engine(const std::string& engine_path);
    void allocate_buffers();
    void preprocess(const cv::Mat& image, float* input_buffer);
    float output_at(int row, int col) const;
    void postprocess(const cv::Size& original_size, std::vector<Detection>& detections);
    void apply_nms(std::vector<Detection>& detections);

public:
    trt_yolov26_detector(const std::string& engine_path,
                         float conf_threshold = 0.25f,
                         float nms_threshold = 0.45f);

    ~trt_yolov26_detector();

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

} // namespace trt_yolov26