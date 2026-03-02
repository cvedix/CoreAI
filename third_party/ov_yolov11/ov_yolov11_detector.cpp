/**
 * @file ov_yolov11_detector.cpp
 * @brief OpenVINO YOLOv11 detector implementation
 */

#ifdef CVEDIX_WITH_OPENVINO

#include "ov_yolov11_detector.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace ov_yolov11 {

ov_yolov11_detector::ov_yolov11_detector(const std::string& model_path,
                                         const std::string& device,
                                         float conf_threshold,
                                         float nms_threshold)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {
    
    // Load model
    if (!load_model(model_path, device)) {
        throw std::runtime_error("Failed to load OpenVINO model: " + model_path);
    }
    
    std::cout << "[ov_yolov11] Loaded model: " << model_path << " on device: " << device << std::endl;
    std::cout << "[ov_yolov11] Input: " << input_width << "x" << input_height 
              << ", Classes: " << num_classes << ", Boxes: " << num_boxes << std::endl;
}

ov_yolov11_detector::~ov_yolov11_detector() {
    // OpenVINO resources are automatically cleaned up
}

bool ov_yolov11_detector::load_model(const std::string& model_path, const std::string& device) {
    try {
        // Read model
        model = core.read_model(model_path);
        
        if (!model) {
            std::cerr << "[ov_yolov11] Failed to read model: " << model_path << std::endl;
            return false;
        }
        
        // Get input/output information
        const auto& input_info = model->input();
        const auto& output_info = model->output();
        
        // Get input shape: [batch, channels, height, width]
        auto input_shape = input_info.get_shape();
        if (input_shape.size() >= 4) {
            input_height = input_shape[2];
            input_width = input_shape[3];
        }
        
        std::cout << "[ov_yolov11] Input shape: [";
        for (size_t i = 0; i < input_shape.size(); i++) {
            std::cout << input_shape[i];
            if (i < input_shape.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
        
        // Get output shape: [batch, dimensions, num_boxes] or [batch, num_boxes, dimensions]
        auto output_shape = output_info.get_shape();
        std::cout << "[ov_yolov11] Output shape: [";
        for (size_t i = 0; i < output_shape.size(); i++) {
            std::cout << output_shape[i];
            if (i < output_shape.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
        
        if (output_shape.size() >= 3) {
            // Check which dimension is larger to determine format
            // YOLOv11 can output [1, 84, 8400] or [1, 8400, 84]
            if (output_shape[1] > output_shape[2]) {
                // Format: [batch, num_boxes, dimensions]
                num_boxes = output_shape[1];
                int dimensions = output_shape[2];
                num_classes = dimensions - 4;
                output_size = dimensions * num_boxes;
                std::cout << "[ov_yolov11] Detected transposed format: [batch, num_boxes, dimensions]" << std::endl;
            } else {
                // Format: [batch, dimensions, num_boxes]
                int dimensions = output_shape[1];  // 4 + num_classes
                num_boxes = output_shape[2];
                num_classes = dimensions - 4;
                output_size = dimensions * num_boxes;
                std::cout << "[ov_yolov11] Detected standard format: [batch, dimensions, num_boxes]" << std::endl;
            }
        }
        
        // Compile model for target device
        compiled_model = core.compile_model(model, device);
        
        // Create infer request
        infer_request = compiled_model.create_infer_request();
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[ov_yolov11] Exception loading model: " << e.what() << std::endl;
        return false;
    }
}

void ov_yolov11_detector::preprocess(const cv::Mat& image, float* input_buffer) {
    // Resize to input size
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(input_width, input_height));
    
    // Convert BGR to RGB
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    
    // Convert to float and normalize to [0, 1]
    cv::Mat float_img;
    rgb.convertTo(float_img, CV_32FC3, 1.0 / 255.0);
    
    // Convert HWC to CHW format
    std::vector<cv::Mat> channels(3);
    cv::split(float_img, channels);
    
    int channel_size = input_height * input_width;
    for (int c = 0; c < 3; c++) {
        memcpy(input_buffer + c * channel_size, channels[c].data, channel_size * sizeof(float));
    }
}

void ov_yolov11_detector::postprocess(float* output, std::vector<Detection>& detections, 
                                       const cv::Size& original_size) {
    detections.clear();
    
    // YOLOv11 output format: [1, 4+num_classes, num_boxes]
    // Row 0-3: x, y, w, h (pixel coordinates relative to input_width x input_height)
    // Row 4+: class scores
    
    int detected_count = 0;
    int threshold_filtered = 0;
    
    // Calculate scale factors (input size -> original image size)
    float scale_x = static_cast<float>(original_size.width) / static_cast<float>(input_width);
    float scale_y = static_cast<float>(original_size.height) / static_cast<float>(input_height);
    
    // Collect all scores for statistics
    std::vector<float> all_max_scores;
    all_max_scores.reserve(num_boxes);
    
    for (int i = 0; i < num_boxes; i++) {
        // Find best class
        float max_score = 0.0f;
        int best_class = 0;
        
        for (int c = 0; c < num_classes; c++) {
            float score = output[(4 + c) * num_boxes + i];
            if (score > max_score) {
                max_score = score;
                best_class = c;
            }
        }
        
        all_max_scores.push_back(max_score);
        
        if (max_score < conf_threshold) {
            threshold_filtered++;
            continue;
        }
        
        // Get bbox (pixel coordinates relative to input size 640x640)
        float cx = output[0 * num_boxes + i];
        float cy = output[1 * num_boxes + i];
        float w = output[2 * num_boxes + i];
        float h = output[3 * num_boxes + i];
        
        // Validate bbox - filter out invalid boxes
        if (cx < 0 || cx > input_width || cy < 0 || cy > input_height) {
            continue;  // Box center outside image
        }
        if (w <= 0 || h <= 0 || w > input_width * 1.5f || h > input_height * 1.5f) {
            continue;  // Box size invalid or too large
        }
        
        // Debug: print first 3 detections
        if (detected_count < 3) {
            std::cout << "[DEBUG] Detection " << detected_count 
                      << ": cx=" << cx << ", cy=" << cy 
                      << ", w=" << w << ", h=" << h 
                      << ", conf=" << max_score 
                      << ", class=" << best_class << std::endl;
        }
        
        // Scale bbox from input size to original image size
        Detection det;
        det.bbox[0] = cx * scale_x;
        det.bbox[1] = cy * scale_y;
        det.bbox[2] = w * scale_x;
        det.bbox[3] = h * scale_y;
        det.conf = max_score;
        det.class_id = best_class;
        
        detections.push_back(det);
        detected_count++;
    }
    
    // Show confidence statistics
    std::sort(all_max_scores.begin(), all_max_scores.end(), std::greater<float>());
    std::cout << "[ov_yolov11] Top 10 confidences: ";
    for (int i = 0; i < std::min(10, (int)all_max_scores.size()); i++) {
        std::cout << all_max_scores[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "[ov_yolov11] Threshold used: " << conf_threshold << std::endl;
    
    std::cout << "[ov_yolov11] Before NMS: " << detected_count << "/" << num_boxes 
              << " boxes passed threshold (filtered: " << threshold_filtered << ")" << std::endl;
    
    // Apply NMS
    apply_nms(detections);
    
    std::cout << "[ov_yolov11] After NMS: " << detections.size() << " detections" << std::endl;
}

void ov_yolov11_detector::apply_nms(std::vector<Detection>& detections) {
    if (detections.empty()) return;
    
    // Sort by confidence descending
    std::sort(detections.begin(), detections.end(), 
              [](const Detection& a, const Detection& b) { return a.conf > b.conf; });
    
    std::vector<bool> suppressed(detections.size(), false);
    std::vector<Detection> result;
    
    for (size_t i = 0; i < detections.size(); i++) {
        if (suppressed[i]) continue;
        
        result.push_back(detections[i]);
        
        // Calculate IoU with remaining boxes
        float x1_a = detections[i].bbox[0] - detections[i].bbox[2] / 2;
        float y1_a = detections[i].bbox[1] - detections[i].bbox[3] / 2;
        float x2_a = detections[i].bbox[0] + detections[i].bbox[2] / 2;
        float y2_a = detections[i].bbox[1] + detections[i].bbox[3] / 2;
        float area_a = detections[i].bbox[2] * detections[i].bbox[3];
        
        for (size_t j = i + 1; j < detections.size(); j++) {
            if (suppressed[j]) continue;
            
            float x1_b = detections[j].bbox[0] - detections[j].bbox[2] / 2;
            float y1_b = detections[j].bbox[1] - detections[j].bbox[3] / 2;
            float x2_b = detections[j].bbox[0] + detections[j].bbox[2] / 2;
            float y2_b = detections[j].bbox[1] + detections[j].bbox[3] / 2;
            float area_b = detections[j].bbox[2] * detections[j].bbox[3];
            
            // Intersection
            float x1_i = std::max(x1_a, x1_b);
            float y1_i = std::max(y1_a, y1_b);
            float x2_i = std::min(x2_a, x2_b);
            float y2_i = std::min(y2_a, y2_b);
            
            float inter_w = std::max(0.0f, x2_i - x1_i);
            float inter_h = std::max(0.0f, y2_i - y1_i);
            float inter_area = inter_w * inter_h;
            
            // IoU
            float iou = inter_area / (area_a + area_b - inter_area + 1e-6f);
            
            if (iou > nms_threshold) {
                suppressed[j] = true;
            }
        }
    }
    
    detections = std::move(result);
}

void ov_yolov11_detector::detect(const cv::Mat& image, std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = std::move(batch_detections[0]);
    }
}

void ov_yolov11_detector::detect(const std::vector<cv::Mat>& images, 
                                  std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());
    
    if (images.empty()) return;
    
    // Process each image (batch size 1 for now)
    for (size_t b = 0; b < images.size(); b++) {
        const cv::Mat& image = images[b];
        cv::Size original_size = image.size();
        
        // Allocate input buffer
        std::vector<float> input_data(3 * input_height * input_width);
        
        // Preprocess
        preprocess(image, input_data.data());
        
        // Get input tensor
        auto input_tensor = infer_request.get_input_tensor();
        
        // Copy preprocessed data to input tensor
        float* input_data_ptr = input_tensor.data<float>();
        std::memcpy(input_data_ptr, input_data.data(), input_data.size() * sizeof(float));
        
        // Run inference
        infer_request.infer();
        
        // Get output tensor
        auto output_tensor = infer_request.get_output_tensor();
        float* output_data = output_tensor.data<float>();
        
        // Postprocess
        postprocess(output_data, detections[b], original_size);
    }
}

cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h) {
    // bbox: center_x, center_y, width, height (already scaled to original image)
    float cx = bbox[0];
    float cy = bbox[1];
    float w = bbox[2];
    float h = bbox[3];
    
    int x = static_cast<int>(cx - w / 2);
    int y = static_cast<int>(cy - h / 2);
    int width = static_cast<int>(w);
    int height = static_cast<int>(h);
    
    // Clip to image bounds
    x = std::max(0, x);
    y = std::max(0, y);
    width = std::min(width, img.cols - x);
    height = std::min(height, img.rows - y);
    
    return cv::Rect(x, y, width, height);
}

} // namespace ov_yolov11

#endif // CVEDIX_WITH_OPENVINO
