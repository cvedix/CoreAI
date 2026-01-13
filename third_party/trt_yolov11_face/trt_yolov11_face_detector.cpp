/**
 * @file trt_yolov11_face_detector.cpp
 * @brief TensorRT YOLOv11 Face detector implementation
 */

#ifdef CVEDIX_WITH_TRT

#include "trt_yolov11_face_detector.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace trt_yolov11_face {

// Logger implementation
void trt_yolov11_face_detector::Logger::log(Severity severity, const char* msg) noexcept {
    if (severity <= Severity::kWARNING) {
        std::cerr << "[TRT-Face] " << msg << std::endl;
    }
}

trt_yolov11_face_detector::trt_yolov11_face_detector(const std::string& engine_path, 
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
    
    std::cout << "[trt_yolov11_face] Loaded engine: " << engine_path << std::endl;
    std::cout << "[trt_yolov11_face] Input: " << input_width << "x" << input_height 
              << ", Classes: " << num_classes << ", Boxes: " << num_boxes 
              << ", Landmarks: " << (with_landmarks ? "Yes" : "No") << std::endl;
}

trt_yolov11_face_detector::~trt_yolov11_face_detector() {
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

bool trt_yolov11_face_detector::load_engine(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[trt_yolov11_face] Engine file not found: " << engine_path << std::endl;
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
        std::cerr << "[trt_yolov11_face] Failed to create TensorRT runtime" << std::endl;
        return false;
    }
    
    engine = runtime->deserializeCudaEngine(engine_data.data(), size);
    if (!engine) {
        std::cerr << "[trt_yolov11_face] Failed to deserialize engine" << std::endl;
        return false;
    }
    
    context = engine->createExecutionContext();
    if (!context) {
        std::cerr << "[trt_yolov11_face] Failed to create execution context" << std::endl;
        return false;
    }
    
    // Get input/output dimensions
    // Input tensor: [batch, 3, height, width]
    auto input_name = engine->getIOTensorName(0);
    auto input_dims = engine->getTensorShape(input_name);
    input_height = input_dims.d[2];
    input_width = input_dims.d[3];
    
    // Output tensor: [batch, dims, num_boxes]
    // dims = 5 for standard detection (4 bbox + 1 class)
    // dims = 20 for detection with landmarks (4 bbox + 1 class + 15 landmarks)
    auto output_name = engine->getIOTensorName(1);
    auto output_dims_shape = engine->getTensorShape(output_name);
    output_dims = output_dims_shape.d[1];
    num_boxes = output_dims_shape.d[2];
    
    // Check if model has landmarks
    // Standard YOLOv11: 4 (bbox) + num_classes
    // YOLOv11-Face with landmarks: 4 (bbox) + num_classes + 15 (5 landmarks * 3 for x, y, visibility) or 10 (5*2)
    if (output_dims > 6) {
        with_landmarks = true;
        // Landmarks are in format: 5 points * 2 coordinates = 10 values
        // So total = 4 (bbox) + 1 (class) + 10 (landmarks) = 15
        // Or could be 4 + 1 + 15 = 20 (with visibility scores)
        num_classes = 1;  // Face detection typically has 1 class
    } else {
        with_landmarks = false;
        num_classes = output_dims - 4;
    }
    
    output_size = output_dims * num_boxes;
    
    return true;
}

void trt_yolov11_face_detector::allocate_buffers() {
    // Input buffer: batch * 3 * height * width * sizeof(float)
    size_t input_size = 1 * 3 * input_height * input_width * sizeof(float);
    cudaMalloc(&device_buffers[0], input_size);
    
    // Output buffer: batch * output_dims * num_boxes * sizeof(float)
    size_t output_buffer_size = 1 * output_size * sizeof(float);
    cudaMalloc(&device_buffers[1], output_buffer_size);
    
    // Host output buffer
    host_output = new float[output_size];
}

void trt_yolov11_face_detector::preprocess(const cv::Mat& image, float* input_buffer) {
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

void trt_yolov11_face_detector::postprocess(float* output, std::vector<FaceDetection>& detections, 
                                             const cv::Size& original_size) {
    detections.clear();
    
    // YOLOv11 output format: [1, output_dims, num_boxes]
    // Row 0-3: x, y, w, h
    // Row 4: class score (for single class - face)
    // Row 5-14: landmarks (if available) - 5 points x 2 coords
    
    float x_factor = static_cast<float>(original_size.width) / input_width;
    float y_factor = static_cast<float>(original_size.height) / input_height;
    
    for (int i = 0; i < num_boxes; i++) {
        // Get confidence score
        float conf = output[4 * num_boxes + i];  // Class score for face
        
        if (conf < conf_threshold) {
            continue;
        }
        
        // Get bbox (relative to input size)
        float cx = output[0 * num_boxes + i];
        float cy = output[1 * num_boxes + i];
        float w = output[2 * num_boxes + i];
        float h = output[3 * num_boxes + i];
        
        FaceDetection det;
        det.bbox[0] = cx * x_factor;
        det.bbox[1] = cy * y_factor;
        det.bbox[2] = w * x_factor;
        det.bbox[3] = h * y_factor;
        det.conf = conf;
        det.class_id = 0;  // Face class
        
        // Extract landmarks if available
        if (with_landmarks && output_dims >= 15) {
            det.has_landmarks = true;
            // Landmarks are typically at indices 5-14 (5 points * 2 coords)
            for (int lm = 0; lm < 5; lm++) {
                int lm_x_idx = (5 + lm * 2) * num_boxes + i;
                int lm_y_idx = (5 + lm * 2 + 1) * num_boxes + i;
                
                if (lm_x_idx < output_size && lm_y_idx < output_size) {
                    det.landmarks[lm * 2] = output[lm_x_idx] * x_factor;
                    det.landmarks[lm * 2 + 1] = output[lm_y_idx] * y_factor;
                } else {
                    det.landmarks[lm * 2] = 0;
                    det.landmarks[lm * 2 + 1] = 0;
                }
            }
        } else {
            det.has_landmarks = false;
        }
        
        detections.push_back(det);
    }
    
    // Apply NMS
    apply_nms(detections);
}

void trt_yolov11_face_detector::apply_nms(std::vector<FaceDetection>& detections) {
    if (detections.empty()) return;
    
    // Sort by confidence descending
    std::sort(detections.begin(), detections.end(), 
              [](const FaceDetection& a, const FaceDetection& b) { return a.conf > b.conf; });
    
    std::vector<bool> suppressed(detections.size(), false);
    std::vector<FaceDetection> result;
    
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

void trt_yolov11_face_detector::detect(const cv::Mat& image, std::vector<FaceDetection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<FaceDetection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = std::move(batch_detections[0]);
    }
}

void trt_yolov11_face_detector::detect(const std::vector<cv::Mat>& images, 
                                        std::vector<std::vector<FaceDetection>>& detections) {
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

cv::Rect get_face_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h) {
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

std::vector<std::pair<int, int>> get_landmarks(const FaceDetection& det) {
    std::vector<std::pair<int, int>> landmarks;
    
    if (det.has_landmarks) {
        for (int i = 0; i < 5; i++) {
            landmarks.emplace_back(
                static_cast<int>(det.landmarks[i * 2]),
                static_cast<int>(det.landmarks[i * 2 + 1])
            );
        }
    }
    
    return landmarks;
}

} // namespace trt_yolov11_face

#endif // CVEDIX_WITH_TRT
