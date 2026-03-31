#pragma once

/**
 * @file ov_yolov11_detector.h
 * @brief OpenVINO YOLOv11 detector for license plate detection
 * 
 * Supports OpenVINO IR (.xml/.bin) and ONNX (.onnx) model files.
 * Optimized for YOLOv11 output format: [batch, 5, num_boxes]
 */

#ifdef CVEDIX_WITH_OPENVINO

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include <openvino/openvino.hpp>

namespace ov_yolov11 {

// Detection result structure
struct Detection {
    float bbox[4];    // center_x, center_y, width, height (relative to input size)
    float conf;       // confidence score
    int class_id;     // class ID
    std::string class_name;  // class name/label
};

/**
 * @brief OpenVINO YOLOv11 detector class
 * 
 * Loads OpenVINO model and performs inference on batches of images.
 * Supports YOLOv11 nano/small/medium/large/xlarge variants.
 */
class ov_yolov11_detector {
private:
    // OpenVINO components
    ov::Core core;
    std::shared_ptr<ov::Model> model;
    ov::CompiledModel compiled_model;
    ov::InferRequest infer_request;
    
    // Model info
    int input_width = 640;
    int input_height = 640;
    int num_classes = 1;
    int num_boxes = 8400;  // YOLOv11 default
    int output_size = 0;
    
    // Letterbox parameters
    float letterbox_scale = 1.0f;   // scale factor for letterbox
    float letterbox_pad_x = 0.0f;   // x offset (padding / 2)
    float letterbox_pad_y = 0.0f;   // y offset (padding / 2)
    
    // Detection parameters
    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;
    
    // Class labels
    std::vector<std::string> labels;
    
    // Internal methods
    bool load_model(const std::string& model_path, const std::string& device);
    bool load_labels(const std::string& labels_path);
    bool load_metadata_yaml(const std::string& yaml_path);
    void preprocess(const cv::Mat& image, float* input_buffer);
    void postprocess(float* output, std::vector<Detection>& detections, const cv::Size& original_size);
    void apply_nms(std::vector<Detection>& detections);

public:
    /**
     * @brief Constructor - loads OpenVINO model
     * @param model_path Path to .xml/.onnx file
     * @param device Device to run on ("CPU", "GPU", "AUTO", etc.)
     * @param conf_threshold Confidence threshold (default: 0.25)
     * @param nms_threshold NMS threshold (default: 0.45)
     * @param labels_path Path to labels file (optional, one label per line)
     */
    ov_yolov11_detector(const std::string& model_path,
                        const std::string& device = "CPU",
                        float conf_threshold = 0.25f,
                        float nms_threshold = 0.45f,
                        const std::string& labels_path = "");
    
    ~ov_yolov11_detector();
    
    /**
     * @brief Run detection on batch of images
     * @param images Input images (BGR format)
     * @param detections Output detections for each image
     */
    void detect(const std::vector<cv::Mat>& images, 
                std::vector<std::vector<Detection>>& detections);
    
    /**
     * @brief Run detection on single image
     * @param image Input image (BGR format)
     * @param detections Output detections
     */
    void detect(const cv::Mat& image, std::vector<Detection>& detections);
    
    // Getters
    int get_input_width() const { return input_width; }
    int get_input_height() const { return input_height; }
    int get_num_classes() const { return num_classes; }
    float get_conf_threshold() const { return conf_threshold; }
    float get_nms_threshold() const { return nms_threshold; }
    const std::vector<std::string>& get_labels() const { return labels; }
    
    // Setters
    void set_conf_threshold(float thresh) { conf_threshold = thresh; }
    void set_nms_threshold(float thresh) { nms_threshold = thresh; }
    void set_labels(const std::vector<std::string>& new_labels) { labels = new_labels; }
};

/**
 * @brief Convert bbox from center coordinates to corner coordinates
 * @param img Original image
 * @param bbox Center coordinates [cx, cy, w, h]
 * @return cv::Rect with corner coordinates
 */
cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h);

} // namespace ov_yolov11

#endif // CVEDIX_WITH_OPENVINO
