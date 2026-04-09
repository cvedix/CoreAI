/**
 * @file cvedix_yolo_ort_detector.h
 * @brief ONNX Runtime-based YOLO detector (generic, supports v5/v8/v11/v12)
 *
 * Uses Microsoft's ONNX Runtime C++ API to load and run ONNX YOLO models.
 * Supports CPU and GPU (CUDA/TensorRT) execution providers.
 * Auto-detects output tensor shape for any YOLO variant.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

namespace cvedix_yolo_ort {

// Detection result structure
struct Detection {
    float bbox[4];    // center_x, center_y, width, height (scaled to original image)
    float conf;       // confidence score
    int class_id;     // class ID
};

/**
 * @brief ONNX Runtime YOLO detector class
 *
 * Loads ONNX models using ORT C++ API and performs inference.
 * Supports:
 * - CPU execution provider (default)
 * - CUDA execution provider (if available)
 * - TensorRT execution provider (if available)
 * - Generic output parsing for any YOLO variant
 */
class cvedix_yolo_ort_detector {
public:
    /**
     * @brief Constructor
     * @param onnx_path Path to ONNX model file
     * @param conf_threshold Confidence threshold
     * @param nms_threshold NMS threshold
     * @param num_classes Number of classes (auto-detected if 0)
     */
    cvedix_yolo_ort_detector(const std::string& onnx_path,
                              float conf_threshold = 0.25f,
                              float nms_threshold = 0.45f,
                              int num_classes = 80);

    ~cvedix_yolo_ort_detector();

    /**
     * @brief Run detection on a batch of images
     */
    void detect(const std::vector<cv::Mat>& images,
                std::vector<std::vector<Detection>>& detections);

    /**
     * @brief Run detection on a single image
     */
    void detect(const cv::Mat& image, std::vector<Detection>& detections);

    // Getters
    int get_input_width() const { return input_width_; }
    int get_input_height() const { return input_height_; }
    int get_num_classes() const { return num_classes_; }
    float get_conf_threshold() const { return conf_threshold_; }
    float get_nms_threshold() const { return nms_threshold_; }

    // Setters
    void set_conf_threshold(float thresh) { conf_threshold_ = thresh; }
    void set_nms_threshold(float thresh) { nms_threshold_ = thresh; }

private:
    // ONNX Runtime objects
    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;

    // Model info
    int input_width_ = 640;
    int input_height_ = 640;
    int num_classes_ = 80;

    // I/O
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;

    // Output analysis
    int num_anchors_ = 8400;   // default for YOLOv11
    bool output_transposed_ = false;  // [1,84,8400] vs [1,8400,84]

    // Letterbox parameters
    float letterbox_scale_ = 1.0f;
    float letterbox_pad_x_ = 0.0f;
    float letterbox_pad_y_ = 0.0f;

    // Thresholds
    float conf_threshold_ = 0.25f;
    float nms_threshold_ = 0.45f;

    // ORT memory info
    Ort::MemoryInfo mem_info_;

    // Internal methods
    bool load_model(const std::string& onnx_path);
    void preprocess(const cv::Mat& image, std::vector<float>& blob);
    void analyze_output_shape();
    void postprocess(std::vector<float>& output_data,
                     const std::vector<int64_t>& output_shape,
                     const cv::Size& original_size,
                     std::vector<Detection>& detections);
    void apply_nms(std::vector<Detection>& detections);

    // Static helpers
    static float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-std::max(-50.0f, std::min(50.0f, x))));
    }

    static float fast_sigmoid(float x) {
        // Clamp then compute
        if (x >= 0) {
            return 1.0f / (1.0f + std::exp(-x));
        } else {
            float z = std::exp(x);
            return z / (1.0f + z);
        }
    }
};

} // namespace cvedix_yolo_ort
