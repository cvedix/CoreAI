#pragma once

/**
 * @file trt_yolov11_detector.h
 * @brief TensorRT YOLOv11 detector with multi-head DFL output support
 *
 * Supports engines with separate reg/cls output heads:
 *   reg1 [1,64,80,80]  cls1 [1,80,80,80]   stride 8
 *   reg2 [1,64,40,40]  cls2 [1,80,40,40]   stride 16
 *   reg3 [1,64,20,20]  cls3 [1,80,20,20]   stride 32
 *
 * Also supports fused single-output engines [1, 84, 8400].
 */

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <cuda_runtime_api.h>

namespace trt_yolov11 {

static constexpr int NUM_SCALES = 3;
static constexpr int DFL_BINS = 16;           // 4 coords × 16 bins = 64 channels
static constexpr int STRIDES[NUM_SCALES] = {8, 16, 32};

// Detection result structure
struct Detection {
    float bbox[4];    // center_x, center_y, width, height (scaled to original image)
    float conf;       // confidence score
    int class_id;     // class ID
};

/**
 * @brief Per-scale output info
 */
struct ScaleInfo {
    int grid_h;           // feature map height
    int grid_w;           // feature map width
    int stride;           // stride for this scale
    int reg_tensor_idx;   // index into engine IO tensors for reg
    int cls_tensor_idx;   // index into engine IO tensors for cls
    size_t reg_size;      // total float count for reg output
    size_t cls_size;      // total float count for cls output
};

/**
 * @brief TensorRT YOLOv11 detector class
 *
 * Supports both multi-head (6 outputs) and fused single-head engines.
 */
class trt_yolov11_detector {
private:
    // TensorRT components
    nvinfer1::IRuntime* runtime = nullptr;
    nvinfer1::ICudaEngine* engine = nullptr;
    nvinfer1::IExecutionContext* context = nullptr;

    // CUDA resources
    cudaStream_t stream;

    // --- Multi-head mode ---
    bool multi_head = false;
    int num_io_tensors = 0;
    std::vector<void*> device_buffers;       // one per IO tensor
    std::vector<float*> host_outputs;        // one per output tensor (index matches device_buffers)
    std::vector<size_t> output_sizes;        // float count per output tensor
    int input_tensor_idx = 0;               // typically 0

    ScaleInfo scales[NUM_SCALES];

    // --- Fused single-head mode (legacy) ---
    float* host_output_fused = nullptr;
    int fused_output_size = 0;
    int fused_num_boxes = 8400;

    // Model info
    int input_width = 640;
    int input_height = 640;
    int num_classes = 80;
    int num_boxes = 8400;        // total anchors across all scales

    // Letterbox parameters
    float letterbox_scale = 1.0f;   // scale factor for letterbox
    float letterbox_pad_x = 0.0f;   // x offset (padding / 2)
    float letterbox_pad_y = 0.0f;   // y offset (padding / 2)

    // Detection parameters
    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;

    // Logger for TensorRT
    class Logger : public nvinfer1::ILogger {
    public:
        void log(Severity severity, const char* msg) noexcept override;
    } logger;

    // Internal methods
    bool load_engine(const std::string& engine_path);
    void allocate_buffers();
    void preprocess(const cv::Mat& image, float* input_buffer);

    // Multi-head postprocess
    void postprocess_multihead(const cv::Size& original_size, std::vector<Detection>& detections);
    // Fused single-head postprocess (legacy)
    void postprocess_fused(float* output, std::vector<Detection>& detections, const cv::Size& original_size);

    void apply_nms(std::vector<Detection>& detections);

    // DFL decode: 64 values → 4 offsets (lt, rt, rb, lb)
    static void dfl_decode(const float* raw, float out[4]);

    // Sigmoid
    static float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

public:
    trt_yolov11_detector(const std::string& engine_path,
                         float conf_threshold = 0.25f,
                         float nms_threshold = 0.45f);

    ~trt_yolov11_detector();

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

} // namespace trt_yolov11
