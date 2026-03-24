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
#include <map>

namespace ov_yolov11 {

ov_yolov11_detector::ov_yolov11_detector(const std::string& model_path,
                                         const std::string& device,
                                         float conf_threshold,
                                         float nms_threshold,
                                         const std::string& labels_path)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {
    

    std::cout << "[ov_yolov11] Runtime version: " << ov::get_openvino_version() << std::endl;
    // Load model
    if (!load_model(model_path, device)) {
        throw std::runtime_error("Failed to load OpenVINO model: " + model_path);
    }
    
    // Try to auto-load metadata.yaml from same directory as model
    std::string metadata_path;
    size_t last_slash = model_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        metadata_path = model_path.substr(0, last_slash + 1) + "metadata.yaml";
    } else {
        metadata_path = "metadata.yaml";
    }
    
    if (load_metadata_yaml(metadata_path)) {
        std::cout << "[ov_yolov11] Loaded " << labels.size() << " class names from metadata.yaml" << std::endl;
    }
    
    // Load labels from file if provided (will override metadata labels)
    if (!labels_path.empty()) {
        if (load_labels(labels_path)) {
            std::cout << "[ov_yolov11] Loaded " << labels.size() << " class labels from file: " << labels_path << std::endl;
        } else {
            std::cerr << "[ov_yolov11] Warning: Failed to load labels from file: " << labels_path << std::endl;
        }
    }
    
    if (labels.empty()) {
        std::cout << "[ov_yolov11] No labels loaded, using generic names (class_0, class_1, ...)" << std::endl;
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
        
        // Get input/output information from original model
        const auto& input_info = model->input();
        const auto& output_info = model->output();
        
        // Get model input shape: [batch, channels, height, width] - NCHW format
        auto model_input_shape = input_info.get_shape();
        if (model_input_shape.size() >= 4) {
            input_height = model_input_shape[2];
            input_width = model_input_shape[3];
        }
        
        std::cout << "[ov_yolov11] Model input shape: [";
        for (size_t i = 0; i < model_input_shape.size(); i++) {
            std::cout << model_input_shape[i];
            if (i < model_input_shape.size() - 1) std::cout << ", ";
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
        
        // ------------- Apply PrePostProcessor (like Python sample) -------------
        ov::preprocess::PrePostProcessor ppp(model);
        
        // 1) Set input tensor information:
        //    - Input tensor will be u8 type (raw image bytes 0-255)
        //    - Layout is NHWC (batch, height, width, channels) - OpenCV format
        ppp.input().tensor()
            .set_element_type(ov::element::u8)
            .set_layout("NHWC");
        
        // 2) Add preprocessing steps:
        //    - Convert BGR to RGB (OpenCV uses BGR, YOLO expects RGB)
        //    - Convert u8 to f32 and normalize to [0, 1] range
        //    - NO auto-resize: we handle letterbox manually
        ppp.input().preprocess()
            .convert_color(ov::preprocess::ColorFormat::RGB)
            .convert_element_type(ov::element::f32)
            .scale(255.0f);
        
        // 2.5) Tell PrePostProcessor that input color format is BGR
        ppp.input().tensor().set_color_format(ov::preprocess::ColorFormat::BGR);
        
        // 3) Set model input layout (model expects NCHW)
        ppp.input().model().set_layout("NCHW");
        
        // 4) Set output tensor type to f32
        ppp.output().tensor().set_element_type(ov::element::f32);
        
        // 5) Build preprocessed model
        model = ppp.build();
        
        std::cout << "[ov_yolov11] PrePostProcessor configured: BGR u8 NHWC -> RGB f32 NCHW, normalized [0,1], manual letterbox" << std::endl;
        
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

// NOTE: This function implements manual letterbox preprocessing to maintain aspect ratio
void ov_yolov11_detector::preprocess(const cv::Mat& image, float* input_buffer) {
    // ---- Letterbox: maintain aspect ratio with padding ----
    float scale = std::min(static_cast<float>(input_width) / image.cols,
                           static_cast<float>(input_height) / image.rows);
    
    int scaled_w = static_cast<int>(image.cols * scale);
    int scaled_h = static_cast<int>(image.rows * scale);
    
    // Center the scaled image in the input canvas
    int pad_left = (input_width - scaled_w) / 2;
    int pad_top = (input_height - scaled_h) / 2;
    
    // Store letterbox parameters for postprocessing
    letterbox_scale = scale;
    letterbox_pad_x = pad_left;
    letterbox_pad_y = pad_top;
    
    // Create a gray canvas (114 is standard YOLO padding color)
    cv::Mat canvas(input_height, input_width, CV_8UC3, cv::Scalar(114, 114, 114));
    
    // Resize image and place it on the canvas
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(scaled_w, scaled_h));
    resized.copyTo(canvas(cv::Rect(pad_left, pad_top, scaled_w, scaled_h)));
    
    // Convert to RGB
    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);
    
    // Convert to float [0, 1]
    cv::Mat float_img;
    rgb.convertTo(float_img, CV_32FC3, 1.0 / 255.0);
    
    // Split channels and copy to input buffer
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
    // Row 0-3: x, y, w, h (pixel coordinates relative to letterbox input)
    // Row 4+: class scores
    
    int detected_count = 0;
    int threshold_filtered = 0;
    
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
        
        // Get bbox (pixel coordinates relative to letterbox input)
        float cx = output[0 * num_boxes + i];
        float cy = output[1 * num_boxes + i];
        float w = output[2 * num_boxes + i];
        float h = output[3 * num_boxes + i];
        
        // ── Remove letterbox padding and scale back to original image ──
        // Convert from center coordinates to xyxy in letterbox space
        float x1 = cx - w * 0.5f;
        float y1 = cy - h * 0.5f;
        float x2 = cx + w * 0.5f;
        float y2 = cy + h * 0.5f;
        
        // Remove padding and scale back to original image space
        x1 = (x1 - letterbox_pad_x) / letterbox_scale;
        y1 = (y1 - letterbox_pad_y) / letterbox_scale;
        x2 = (x2 - letterbox_pad_x) / letterbox_scale;
        y2 = (y2 - letterbox_pad_y) / letterbox_scale;
        
        // Clamp to valid image bounds
        x1 = std::max(0.0f, std::min(x1, (float)original_size.width));
        y1 = std::max(0.0f, std::min(y1, (float)original_size.height));
        x2 = std::max(0.0f, std::min(x2, (float)original_size.width));
        y2 = std::max(0.0f, std::min(y2, (float)original_size.height));
        
        // Convert back to center coordinates
        float final_cx = (x1 + x2) * 0.5f;
        float final_cy = (y1 + y2) * 0.5f;
        float final_w = (x2 - x1);
        float final_h = (y2 - y1);
        
        // Debug: print first 3 detections
        if (detected_count < 3) {
            std::cout << "[DEBUG] Detection " << detected_count 
                      << ": cx=" << final_cx << ", cy=" << final_cy 
                      << ", w=" << final_w << ", h=" << final_h 
                      << ", conf=" << max_score 
                      << ", class=" << best_class << std::endl;
        }
        
        Detection det;
        det.bbox[0] = final_cx;
        det.bbox[1] = final_cy;
        det.bbox[2] = final_w;
        det.bbox[3] = final_h;
        det.conf = max_score;
        det.class_id = best_class;
        
        // Map class_id to class_name
        if (best_class >= 0 && best_class < static_cast<int>(labels.size()) && !labels[best_class].empty()) {
            det.class_name = labels[best_class];
        } else {
            det.class_name = "class_" + std::to_string(best_class);
        }
        
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
    
    // Process each image
    for (size_t b = 0; b < images.size(); b++) {
        const cv::Mat& image = images[b];
        cv::Size original_size = image.size();
        
        // Apply letterbox preprocessing to maintain aspect ratio
        std::vector<float> input_data(3 * input_height * input_width);
        preprocess(image, input_data.data());
        
        // Create letterbox image (u8 for PrePostProcessor)
        cv::Mat letterbox_img(input_height, input_width, CV_8UC3);
        for (int c = 0; c < 3; c++) {
            for (int h = 0; h < input_height; h++) {
                for (int w = 0; w < input_width; w++) {
                    int idx = h * input_width + w;
                    letterbox_img.at<cv::Vec3b>(h, w)[c] = 
                        static_cast<uint8_t>(input_data[c * input_height * input_width + idx] * 255.0f);
                }
            }
        }
        
        // Ensure image is continuous and BGR u8 format
        cv::Mat input_image;
        if (!letterbox_img.isContinuous()) {
            input_image = letterbox_img.clone();
        } else {
            input_image = letterbox_img;
        }
        
        // Create input tensor with shape [1, H, W, C] (NHWC format)
        ov::Shape input_shape = {1, static_cast<size_t>(input_height), 
                                  static_cast<size_t>(input_width), 3};
        
        // Create tensor from raw image data (u8)
        ov::Tensor input_tensor(ov::element::u8, input_shape, input_image.data);
        
        // Set input tensor
        infer_request.set_input_tensor(input_tensor);
        
        // Run inference
        infer_request.infer();
        
        // Get output tensor
        auto output_tensor = infer_request.get_output_tensor();
        float* output_data = output_tensor.data<float>();
        
        // Postprocess
        postprocess(output_data, detections[b], original_size);
    }
}

bool ov_yolov11_detector::load_labels(const std::string& labels_path) {
    labels.clear();
    
    std::ifstream file(labels_path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (!line.empty()) {
            labels.push_back(line);
        }
    }
    
    file.close();
    return !labels.empty();
}

bool ov_yolov11_detector::load_metadata_yaml(const std::string& yaml_path) {
    std::ifstream file(yaml_path);
    if (!file.is_open()) {
        return false;
    }
    
    labels.clear();
    std::map<int, std::string> class_map;
    
    std::string line;
    bool in_names_section = false;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Check if we're entering names section
        if (line == "names:") {
            in_names_section = true;
            continue;
        }
        
        // Check if we're leaving names section (new section starts)
        if (in_names_section && !line.empty() && line.find(':') != std::string::npos && line[0] != ' ') {
            if (line.find("  ") != 0) {  // Not indented = new section
                break;
            }
        }
        
        // Parse name entries: "  0: person" or "  10: fire hydrant"
        if (in_names_section && line.find(':') != std::string::npos) {
            size_t colon_pos = line.find(':');
            std::string id_str = line.substr(0, colon_pos);
            std::string name = line.substr(colon_pos + 1);
            
            // Trim
            id_str.erase(0, id_str.find_first_not_of(" \t"));
            id_str.erase(id_str.find_last_not_of(" \t") + 1);
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            
            try {
                int class_id = std::stoi(id_str);
                class_map[class_id] = name;
            } catch (...) {
                // Skip invalid entries
            }
        }
    }
    
    file.close();
    
    // Convert map to vector (sorted by class_id)
    if (!class_map.empty()) {
        int max_id = class_map.rbegin()->first;
        labels.resize(max_id + 1);
        
        for (const auto& pair : class_map) {
            labels[pair.first] = pair.second;
        }
        
        return true;
    }
    
    return false;
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
