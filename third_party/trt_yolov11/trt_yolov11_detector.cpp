/**
 * @file trt_yolov11_detector.cpp
 * @brief TensorRT YOLOv11 detector implementation
 */

#ifdef CVEDIX_WITH_TRT

#include "trt_yolov11_detector.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace trt_yolov11 {

// Logger implementation
void trt_yolov11_detector::Logger::log(Severity severity, const char* msg) noexcept {
    if (severity <= Severity::kWARNING) {
        std::cerr << "[TRT] " << msg << std::endl;
    }
}

trt_yolov11_detector::trt_yolov11_detector(const std::string& engine_path, 
                                           float conf_threshold,
                                           float nms_threshold)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {
    
    // Initialize CUDA stream
    cudaStreamCreate(&stream);
    
    // Load engine
    if (!load_engine(engine_path)) {
        throw std::runtime_error("Failed to load TensorRT engine: " + engine_path);
    }
    
    // Allocate buffers
    allocate_buffers();
    
    std::cout << "[trt_yolov11] Loaded engine: " << engine_path << std::endl;
    std::cout << "[trt_yolov11] Input: " << input_width << "x" << input_height 
              << ", Classes: " << num_classes << ", Boxes: " << num_boxes << std::endl;
}

trt_yolov11_detector::~trt_yolov11_detector() {
    // Free CUDA resources
    cudaStreamDestroy(stream);
    
    if (device_buffers[0]) cudaFree(device_buffers[0]);
    if (device_buffers[1]) cudaFree(device_buffers[1]);
    if (host_output) delete[] host_output;
    
    // Free TensorRT resources
    if (context) delete context;
    if (engine) delete engine;
    if (runtime) delete runtime;
}

bool trt_yolov11_detector::load_engine(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[trt_yolov11] Engine file not found: " << engine_path << std::endl;
        return false;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read engine data
    std::vector<char> engine_data(size);
    file.read(engine_data.data(), size);
    file.close();
    
    // Create runtime and deserialize engine
    runtime = nvinfer1::createInferRuntime(logger);
    if (!runtime) {
        std::cerr << "[trt_yolov11] Failed to create TensorRT runtime" << std::endl;
        return false;
    }
    
    engine = runtime->deserializeCudaEngine(engine_data.data(), size);
    if (!engine) {
        std::cerr << "[trt_yolov11] Failed to deserialize engine" << std::endl;
        return false;
    }
    
    context = engine->createExecutionContext();
    if (!context) {
        std::cerr << "[trt_yolov11] Failed to create execution context" << std::endl;
        return false;
    }
    
    // Get input/output dimensions
    // Input tensor: [batch, 3, height, width]
    auto input_name = engine->getIOTensorName(0);
    auto input_dims = engine->getTensorShape(input_name);
    input_height = input_dims.d[2];
    input_width = input_dims.d[3];
    
    // Output tensor: [batch, 5, num_boxes] for 1 class
    auto output_name = engine->getIOTensorName(1);
    auto output_dims = engine->getTensorShape(output_name);
    int dimensions = output_dims.d[1];  // 4 + num_classes
    num_boxes = output_dims.d[2];
    num_classes = dimensions - 4;
    
    output_size = dimensions * num_boxes;
    
    return true;
}

void trt_yolov11_detector::allocate_buffers() {
    // Input buffer: batch * 3 * height * width * sizeof(float)
    size_t input_size = 1 * 3 * input_height * input_width * sizeof(float);
    cudaMalloc(&device_buffers[0], input_size);
    
    // Output buffer: batch * (4 + num_classes) * num_boxes * sizeof(float)
    size_t output_buffer_size = 1 * output_size * sizeof(float);
    cudaMalloc(&device_buffers[1], output_buffer_size);
    
    // Host output buffer
    host_output = new float[output_size];
}

void trt_yolov11_detector::preprocess(const cv::Mat& image, float* input_buffer) {
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

void trt_yolov11_detector::postprocess(float* output, std::vector<Detection>& detections, 
                                        const cv::Size& original_size) {
    detections.clear();
    
    // YOLOv11 output format: [1, 5, 8400]
    // Row 0-3: x, y, w, h
    // Row 4+: class scores
    
    float x_factor = static_cast<float>(original_size.width) / input_width;
    float y_factor = static_cast<float>(original_size.height) / input_height;
    
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
        
        if (max_score < conf_threshold) {
            continue;
        }
        
        // Get bbox (relative to input size)
        float cx = output[0 * num_boxes + i];
        float cy = output[1 * num_boxes + i];
        float w = output[2 * num_boxes + i];
        float h = output[3 * num_boxes + i];
        
        Detection det;
        det.bbox[0] = cx * x_factor;
        det.bbox[1] = cy * y_factor;
        det.bbox[2] = w * x_factor;
        det.bbox[3] = h * y_factor;
        det.conf = max_score;
        det.class_id = best_class;
        
        detections.push_back(det);
    }
    
    // Apply NMS
    apply_nms(detections);
}

void trt_yolov11_detector::apply_nms(std::vector<Detection>& detections) {
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

void trt_yolov11_detector::detect(const cv::Mat& image, std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = std::move(batch_detections[0]);
    }
}

void trt_yolov11_detector::detect(const std::vector<cv::Mat>& images, 
                                   std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());
    
    if (images.empty()) return;
    
    // Process each image (batch size 1 for now)
    for (size_t b = 0; b < images.size(); b++) {
        const cv::Mat& image = images[b];
        cv::Size original_size = image.size();
        
        // Allocate temporary input buffer
        std::vector<float> input_data(3 * input_height * input_width);
        
        // Preprocess
        preprocess(image, input_data.data());
        
        // Copy to device
        cudaMemcpyAsync(device_buffers[0], input_data.data(), 
                        input_data.size() * sizeof(float),
                        cudaMemcpyHostToDevice, stream);
        
        // Set tensor addresses
        auto input_name = engine->getIOTensorName(0);
        auto output_name = engine->getIOTensorName(1);
        context->setTensorAddress(input_name, device_buffers[0]);
        context->setTensorAddress(output_name, device_buffers[1]);
        
        // Run inference
        context->enqueueV3(stream);
        
        // Copy output to host
        cudaMemcpyAsync(host_output, device_buffers[1], 
                        output_size * sizeof(float),
                        cudaMemcpyDeviceToHost, stream);
        
        // Synchronize
        cudaStreamSynchronize(stream);
        
        // Postprocess
        postprocess(host_output, detections[b], original_size);
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

} // namespace trt_yolov11

#endif // CVEDIX_WITH_TRT
