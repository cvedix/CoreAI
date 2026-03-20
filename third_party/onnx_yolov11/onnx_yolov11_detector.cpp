/**
 * @file onnx_yolov11_detector.cpp
 * @brief OpenCV DNN-based YOLOv11 detector implementation
 */

#include "onnx_yolov11_detector.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>

namespace onnx_yolov11 {

// ──────────────────────── Constructor ──────────────────
onnx_yolov11_detector::onnx_yolov11_detector(const std::string& onnx_path,
                                             float conf_threshold,
                                             float nms_threshold)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {

    if (!load_model(onnx_path)) {
        throw std::runtime_error("Failed to load ONNX model: " + onnx_path);
    }

    std::cout << "[onnx_yolov11] Loaded ONNX model: " << onnx_path << std::endl;
    std::cout << "[onnx_yolov11] Input: " << input_width << "x" << input_height
              << ", Classes: " << num_classes << std::endl;
}

// ──────────────────────── Destructor ──────────────────
onnx_yolov11_detector::~onnx_yolov11_detector() {
    // OpenCV manages memory automatically
}

// ──────────────────────── load_model ──────────────────
bool onnx_yolov11_detector::load_model(const std::string& onnx_path) {
    // Check if file exists
    std::ifstream file(onnx_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[onnx_yolov11] Model file not found: " << onnx_path << std::endl;
        return false;
    }
    file.close();

    try {
        // Load ONNX model using OpenCV DNN
        net = cv::dnn::readNetFromONNX(onnx_path);
        if (net.empty()) {
            std::cerr << "[onnx_yolov11] Failed to load ONNX model" << std::endl;
            return false;
        }

        // Set backend to CPU
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        // Get output layer name(s)
        std::vector<std::string> output_names = net.getUnconnectedOutLayersNames();
        if (output_names.empty()) {
            std::cerr << "[onnx_yolov11] No output layers found" << std::endl;
            return false;
        }

        output_layer_name = output_names[0];
        
        std::cout << "[onnx_yolov11] Output layer: " << output_layer_name << std::endl;

        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "[onnx_yolov11] OpenCV DNN Error: " << e.what() << std::endl;
        return false;
    }
}

// ──────────────────────── preprocess ──────────────────
void onnx_yolov11_detector::preprocess(const cv::Mat& image, cv::Mat& blob) {
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(input_width, input_height));

    // // Convert to RGB if needed
    // cv::Mat rgb;
    // if (image.channels() == 3) {
    //     cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    // } else {
    //     rgb = resized;
    // }

    // Create blob with normalization (0-1 range)
    blob = cv::dnn::blobFromImage(resized, 1.0 / 255.0, cv::Size(input_width, input_height),
                                  cv::Scalar(0, 0, 0), true, false, CV_32F);
}

// ──────────────────────── postprocess ──────────────────
void onnx_yolov11_detector::postprocess(cv::Mat& output, const cv::Size& original_size,
                                         std::vector<Detection>& detections) {
    detections.clear();

    float x_factor = static_cast<float>(original_size.width) / input_width;
    float y_factor = static_cast<float>(original_size.height) / input_height;

    // Output format: [1, num_detections, 84]
    // Each detection: [x, y, w, h, obj_conf, class_scores...]

    // output is typically [1, 8400, 84] after forward pass
    int num_detections = output.rows;
    int data_size = output.cols;

    for (int i = 0; i < num_detections; i++) {
        float* row = output.ptr<float>(i);

        // Extract bbox and objectness
        float cx = row[0];
        float cy = row[1];
        float w = row[2];
        float h = row[3];
        float max_class_conf = 0.0f;
        int best_class = 0;

        for (int c = 0; c < num_classes; c++) {
            float class_conf = row[4 + c];   // 🔥 bắt đầu từ 4
            if (class_conf > max_class_conf) {
                max_class_conf = class_conf;
                best_class = c;
            }
        }

        float conf = max_class_conf;

        if (conf < conf_threshold) {
            continue;
        }

        Detection det;
        det.bbox[0] = cx * x_factor;
        det.bbox[1] = cy * y_factor;
        det.bbox[2] = w * x_factor;
        det.bbox[3] = h * y_factor;
        det.conf = conf;
        det.class_id = best_class;

        detections.push_back(det);
    }

    apply_nms(detections);
}

// ──────────────────────── apply_nms ──────────────────
void onnx_yolov11_detector::apply_nms(std::vector<Detection>& detections) {
    if (detections.empty()) return;

    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) { return a.conf > b.conf; });

    std::vector<bool> suppressed(detections.size(), false);
    std::vector<Detection> result;

    for (size_t i = 0; i < detections.size(); i++) {
        if (suppressed[i]) continue;
        result.push_back(detections[i]);

        for (size_t j = i + 1; j < detections.size(); j++) {
            if (suppressed[j]) continue;

            // Calculate IoU between detections[i] and detections[j]
            float x1_min = detections[i].bbox[0] - detections[i].bbox[2] / 2.0f;
            float y1_min = detections[i].bbox[1] - detections[i].bbox[3] / 2.0f;
            float x1_max = detections[i].bbox[0] + detections[i].bbox[2] / 2.0f;
            float y1_max = detections[i].bbox[1] + detections[i].bbox[3] / 2.0f;

            float x2_min = detections[j].bbox[0] - detections[j].bbox[2] / 2.0f;
            float y2_min = detections[j].bbox[1] - detections[j].bbox[3] / 2.0f;
            float x2_max = detections[j].bbox[0] + detections[j].bbox[2] / 2.0f;
            float y2_max = detections[j].bbox[1] + detections[j].bbox[3] / 2.0f;

            float inter_xmin = std::max(x1_min, x2_min);
            float inter_ymin = std::max(y1_min, y2_min);
            float inter_xmax = std::min(x1_max, x2_max);
            float inter_ymax = std::min(y1_max, y2_max);

            if (inter_xmin < inter_xmax && inter_ymin < inter_ymax) {
                float inter_area = (inter_xmax - inter_xmin) * (inter_ymax - inter_ymin);
                float box1_area = detections[i].bbox[2] * detections[i].bbox[3];
                float box2_area = detections[j].bbox[2] * detections[j].bbox[3];
                float union_area = box1_area + box2_area - inter_area;

                float iou = inter_area / union_area;

                if (iou > nms_threshold) {
                    suppressed[j] = true;
                }
            }
        }
    }

    detections = std::move(result);
}

// ──────────────────────── detect (single) ──────────────
void onnx_yolov11_detector::detect(const cv::Mat& image, std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = batch_detections[0];
    }
}

// ──────────────────────── detect (batch) ──────────────
void onnx_yolov11_detector::detect(const std::vector<cv::Mat>& images,
                                    std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());

    if (images.empty()) return;

    for (size_t b = 0; b < images.size(); b++) {
        if (images[b].empty()) continue;

        cv::Mat blob;
        preprocess(images[b], blob);

        // Set input and forward
        net.setInput(blob);
        cv::Mat output = net.forward(output_layer_name);

        // Handle different output shapes
        // Reshape if needed: some models output [1, num_detections, 84] or [1, 84, num_detections]
        if (output.dims == 3) {
            if (output.size[1] == 84 && output.size[2] == 8400) {
                // Shape: [1, 84, 8400] - transpose to [8400, 84]
                cv::Mat temp = output.reshape(0, 84);  // Reshape to [84, 8400]
                output = temp.t();  // Transpose to [8400, 84]
            } else {
                // Shape: [1, num_detections, 84]
                output = output.reshape(1, output.size[1]);  // [num_detections, 84]
            }
        } else if (output.dims == 2 && output.rows == 1) {
            // Shape: [1, num_detections * 84]
            int num_detections = output.cols / (4 + num_classes);
            output = output.reshape(1, num_detections);  // [num_detections, 84]
        }

        postprocess(output, images[b].size(), detections[b]);
    }
}

// ──────────────────────── get_rect ──────────────────
cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h) {
    float cx = bbox[0];
    float cy = bbox[1];
    float w = bbox[2];
    float h = bbox[3];

    int x = static_cast<int>(cx - w / 2);
    int y = static_cast<int>(cy - h / 2);
    int width = static_cast<int>(w);
    int height = static_cast<int>(h);

    x = std::max(0, x);
    y = std::max(0, y);
    width = std::min(width, img.cols - x);
    height = std::min(height, img.rows - y);

    return cv::Rect(x, y, width, height);
}

} // namespace onnx_yolov11
