#pragma once

/**
 * @file trt_yolov11_face_detector.h
 * @brief TensorRT YOLOv11 detector for face detection
 * 
 * Supports pre-built .engine files with FP16 precision.
 * Optimized for YOLOv11-Face output format.
 * 
 * YOLOv11-Face output format options:
 * 1. Standard detection: [batch, 5, num_boxes] (4 bbox + 1 class)
 * 2. With landmarks: [batch, 20, num_boxes] (4 bbox + 1 class + 15 landmarks)
 */

#ifdef CVEDIX_WITH_TRT

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <cuda_runtime_api.h>

namespace trt_yolov11_face {

// Face detection result structure
struct FaceDetection {
    float bbox[4];          // center_x, center_y, width, height (relative to original image)
    float conf;             // confidence score
    int class_id;           // class ID (typically 0 for face)
    
    // 5-point facial landmarks (if model supports)
    // Order: right_eye, left_eye, nose, right_mouth, left_mouth
    bool has_landmarks = false;
    float landmarks[10];    // 5 points x 2 coordinates (x, y)
};

/**
 * @brief TensorRT YOLOv11 Face detector class
 * 
 * Loads TensorRT engine and performs face detection on images.
 * Supports YOLOv11 nano/small/medium/large/xlarge variants trained for face detection.
 */
class trt_yolov11_face_detector {
private:
    // TensorRT components
    nvinfer1::IRuntime* runtime = nullptr;
    nvinfer1::ICudaEngine* engine = nullptr;
    nvinfer1::IExecutionContext* context = nullptr;
    
    // CUDA resources
    cudaStream_t stream;
    void* device_buffers[2];  // input and output buffers
    float* host_output = nullptr;
    
    // Model info
    int input_width = 640;
    int input_height = 640;
    int num_classes = 1;      // Face class only
    int num_boxes = 8400;     // YOLOv11 default
    int output_dims = 5;      // 4 (bbox) + 1 (class) or 4 + 1 + 15 (with landmarks)
    int output_size = 0;
    bool with_landmarks = false;
    
    // Detection parameters
    float conf_threshold = 0.5f;
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
    void postprocess(float* output, std::vector<FaceDetection>& detections, const cv::Size& original_size);
    void apply_nms(std::vector<FaceDetection>& detections);

public:
    /**
     * @brief Constructor - loads TensorRT engine
     * @param engine_path Path to .engine file
     * @param conf_threshold Confidence threshold (default: 0.5)
     * @param nms_threshold NMS threshold (default: 0.45)
     */
    trt_yolov11_face_detector(const std::string& engine_path, 
                               float conf_threshold = 0.5f,
                               float nms_threshold = 0.45f);
    
    ~trt_yolov11_face_detector();
    
    /**
     * @brief Run face detection on batch of images
     * @param images Input images (BGR format)
     * @param detections Output detections for each image
     */
    void detect(const std::vector<cv::Mat>& images, 
                std::vector<std::vector<FaceDetection>>& detections);
    
    /**
     * @brief Run face detection on single image
     * @param image Input image (BGR format)
     * @param detections Output detections
     */
    void detect(const cv::Mat& image, std::vector<FaceDetection>& detections);
    
    // Getters
    int get_input_width() const { return input_width; }
    int get_input_height() const { return input_height; }
    int get_num_classes() const { return num_classes; }
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }
    bool has_landmarks() const { return with_landmarks; }
    
    // Setters
    void set_conf_threshold(float thresh) { conf_threshold = thresh; }
    void set_nms_threshold(float thresh) { nms_threshold = thresh; }
};

/**
 * @brief Convert bbox from center coordinates to corner coordinates
 * @param img Original image
 * @param bbox Center coordinates [cx, cy, w, h]
 * @param input_w Model input width
 * @param input_h Model input height
 * @return cv::Rect with corner coordinates
 */
cv::Rect get_face_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h);

/**
 * @brief Extract landmarks as vector of points
 * @param det Face detection with landmarks
 * @return Vector of 5 landmark points (x, y pairs)
 */
std::vector<std::pair<int, int>> get_landmarks(const FaceDetection& det);

} // namespace trt_yolov11_face

#endif // CVEDIX_WITH_TRT
