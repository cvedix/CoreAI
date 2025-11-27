#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include "rknn_api.h"

// Model configuration
int INPUT_SIZE = 320;  // Will be updated from model attributes
float CONF_THRESHOLD = 0.6f;  // Made non-const to allow adjustment
const float NMS_THRESHOLD = 0.3f;

// Face detection result structure
struct FaceDetection {
    cv::Rect bbox;           // Bounding box (x, y, width, height)
    float score;              // Confidence score
    std::vector<cv::Point2f> landmarks;  // 5 facial landmarks
};

// Prior box structure (for YuNet decoding)
struct PriorBox {
    float cx, cy;  // Center coordinates (normalized 0-1)
    float w, h;    // Width and height (normalized 0-1)
};

// Generate priors for YuNet (same as OpenCV FaceDetectorYN)
std::vector<PriorBox> generate_priors(int input_w, int input_h) {
    std::vector<PriorBox> priors;
    
    // Calculate feature map sizes (same as YuNet)
    int feature_map_2nd_w = ((input_w + 1) / 2) / 2;
    int feature_map_2nd_h = ((input_h + 1) / 2) / 2;
    int feature_map_3rd_w = feature_map_2nd_w / 2;
    int feature_map_3rd_h = feature_map_2nd_h / 2;
    int feature_map_4th_w = feature_map_3rd_w / 2;
    int feature_map_4th_h = feature_map_3rd_h / 2;
    int feature_map_5th_w = feature_map_4th_w / 2;
    int feature_map_5th_h = feature_map_4th_h / 2;
    int feature_map_6th_w = feature_map_5th_w / 2;
    int feature_map_6th_h = feature_map_5th_h / 2;
    
    std::vector<std::pair<int, int>> feature_map_sizes = {
        {feature_map_3rd_w, feature_map_3rd_h},
        {feature_map_4th_w, feature_map_4th_h},
        {feature_map_5th_w, feature_map_5th_h},
        {feature_map_6th_w, feature_map_6th_h}
    };
    
    // Min sizes for each feature map (same as YuNet)
    std::vector<std::vector<float>> min_sizes = {
        {10.0f, 16.0f, 24.0f},
        {32.0f, 48.0f},
        {64.0f, 96.0f},
        {128.0f, 192.0f, 256.0f}
    };
    
    std::vector<int> steps = {8, 16, 32, 64};
    
    // Generate priors
    for (size_t i = 0; i < feature_map_sizes.size(); ++i) {
        int map_w = feature_map_sizes[i].first;
        int map_h = feature_map_sizes[i].second;
        int step = steps[i];
        
        for (int h = 0; h < map_h; ++h) {
            for (int w = 0; w < map_w; ++w) {
                for (size_t j = 0; j < min_sizes[i].size(); ++j) {
                    float s_kx = min_sizes[i][j] / input_w;
                    float s_ky = min_sizes[i][j] / input_h;
                    
                    float cx = (w + 0.5f) * step / input_w;
                    float cy = (h + 0.5f) * step / input_h;
                    
                    PriorBox prior;
                    prior.cx = cx;
                    prior.cy = cy;
                    prior.w = s_kx;
                    prior.h = s_ky;
                    priors.push_back(prior);
                }
            }
        }
    }
    
    return priors;
}

// Letterbox parameters
struct LetterboxParams {
    float scale;
    int pad_w;
    int pad_h;
};

// Calculate letterbox parameters
LetterboxParams calculate_letterbox(int src_w, int src_h, int dst_w, int dst_h) {
    LetterboxParams params;
    float scale_w = static_cast<float>(dst_w) / src_w;
    float scale_h = static_cast<float>(dst_h) / src_h;
    params.scale = std::min(scale_w, scale_h);
    
    int new_w = static_cast<int>(src_w * params.scale);
    int new_h = static_cast<int>(src_h * params.scale);
    
    params.pad_w = (dst_w - new_w) / 2;
    params.pad_h = (dst_h - new_h) / 2;
    
    return params;
}

// Apply letterbox to image
cv::Mat letterbox(const cv::Mat& img, int dst_w, int dst_h, LetterboxParams& params) {
    params = calculate_letterbox(img.cols, img.rows, dst_w, dst_h);
    
    int new_w = static_cast<int>(img.cols * params.scale);
    int new_h = static_cast<int>(img.rows * params.scale);
    
    cv::Mat resized;
    cv::resize(img, resized, cv::Size(new_w, new_h));
    
    cv::Mat letterboxed = cv::Mat::zeros(dst_h, dst_w, img.type());
    resized.copyTo(letterboxed(cv::Rect(params.pad_w, params.pad_h, new_w, new_h)));
    
    return letterboxed;
}

// Sigmoid function
float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

// NMS function
void nms(std::vector<cv::Rect>& boxes, std::vector<float>& scores, 
         std::vector<std::vector<cv::Point2f>>& landmarks, float threshold) {
    std::vector<int> indices;
    std::vector<std::pair<float, int>> score_index;
    
    for (size_t i = 0; i < scores.size(); i++) {
        score_index.push_back(std::make_pair(scores[i], i));
    }
    
    std::sort(score_index.begin(), score_index.end(), 
              [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                  return a.first > b.first;
              });
    
    std::vector<bool> suppressed(scores.size(), false);
    
    for (size_t i = 0; i < score_index.size(); i++) {
        int idx = score_index[i].second;
        if (suppressed[idx]) continue;
        
        indices.push_back(idx);
        
        for (size_t j = i + 1; j < score_index.size(); j++) {
            int idx2 = score_index[j].second;
            if (suppressed[idx2]) continue;
            
            cv::Rect& box1 = boxes[idx];
            cv::Rect& box2 = boxes[idx2];
            
            int x1 = std::max(box1.x, box2.x);
            int y1 = std::max(box1.y, box2.y);
            int x2 = std::min(box1.x + box1.width, box2.x + box2.width);
            int y2 = std::min(box1.y + box1.height, box2.y + box2.height);
            
            int inter_area = std::max(0, x2 - x1) * std::max(0, y2 - y1);
            int box1_area = box1.width * box1.height;
            int box2_area = box2.width * box2.height;
            float iou = static_cast<float>(inter_area) / (box1_area + box2_area - inter_area);
            
            if (iou > threshold) {
                suppressed[idx2] = true;
            }
        }
    }
    
    std::vector<cv::Rect> filtered_boxes;
    std::vector<float> filtered_scores;
    std::vector<std::vector<cv::Point2f>> filtered_landmarks;
    
    for (int idx : indices) {
        filtered_boxes.push_back(boxes[idx]);
        filtered_scores.push_back(scores[idx]);
        filtered_landmarks.push_back(landmarks[idx]);
    }
    
    boxes = filtered_boxes;
    scores = filtered_scores;
    landmarks = filtered_landmarks;
}

// Process single frame - returns detected faces with bounding boxes
std::vector<FaceDetection> process_frame(cv::Mat& frame, rknn_context ctx, const rknn_input_output_num& io_num,
                   const rknn_tensor_attr* input_attrs, const rknn_tensor_attr* output_attrs, int model_input_size) {
    // Preprocess image
    LetterboxParams lb_params;
    cv::Mat letterboxed = letterbox(frame, model_input_size, model_input_size, lb_params);
    
    // Convert to RGB
    cv::Mat rgb;
    cv::cvtColor(letterboxed, rgb, cv::COLOR_BGR2RGB);
    
    // Get input size from model attributes
    int input_size_bytes = input_attrs[0].size;
    int expected_size = model_input_size * model_input_size * 3;
    
    // Prepare input
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = input_attrs[0].type;
    inputs[0].size = input_size_bytes;  // Use size from model attributes
    inputs[0].fmt = input_attrs[0].fmt;
    inputs[0].buf = rgb.data;
    
    // Log input info for debugging
    if (input_size_bytes != expected_size) {
        std::cerr << "WARNING: Model expects " << input_size_bytes << " bytes, but calculated " 
                  << expected_size << " bytes for " << model_input_size << "x" << model_input_size << "x3" << std::endl;
    }
    
    int ret = rknn_inputs_set(ctx, io_num.n_input, inputs);
    if (ret != RKNN_SUCC) {
        std::cerr << "rknn_inputs_set failed: " << ret << std::endl;
        return std::vector<FaceDetection>();  // Return empty vector on error
    }
    
    // Run inference
    ret = rknn_run(ctx, nullptr);
    if (ret != RKNN_SUCC) {
        std::cerr << "rknn_run failed: " << ret << std::endl;
        return std::vector<FaceDetection>();  // Return empty vector on error
    }
    
    // Get outputs
    rknn_output outputs[io_num.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        outputs[i].want_float = 1;
    }
    
    ret = rknn_outputs_get(ctx, io_num.n_output, outputs, nullptr);
    if (ret != RKNN_SUCC) {
        std::cerr << "rknn_outputs_get failed: " << ret << std::endl;
        return std::vector<FaceDetection>();  // Return empty vector on error
    }
    
    // Post-process outputs
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<std::vector<cv::Point2f>> landmarks;
    
    // Debug: Log output shapes (only once at start)
    static bool logged_shapes = false;
    if (!logged_shapes) {
        std::cout << "DEBUG: Output shapes:" << std::endl;
        for (uint32_t i = 0; i < io_num.n_output; i++) {
            std::cout << "  Output[" << i << "]: n_dims=" << output_attrs[i].n_dims << ", dims=[";
            for (uint32_t d = 0; d < output_attrs[i].n_dims; d++) {
                if (d > 0) std::cout << ", ";
                std::cout << output_attrs[i].dims[d];
            }
            std::cout << "], size=" << output_attrs[i].size << " bytes" << std::endl;
        }
        logged_shapes = true;
    }
    
    // YuNet outputs format: loc [num_priors, 14], conf [num_priors, 2], iou [num_priors, 1]
    // But model might output differently - check actual format
    int num_priors = 1;
    if (output_attrs[0].n_dims >= 2) {
        num_priors = output_attrs[0].dims[0] > 1 ? output_attrs[0].dims[0] : output_attrs[0].dims[1];
    } else if (output_attrs[0].n_dims == 1) {
        num_priors = output_attrs[0].dims[0];
    }
    
    // Check output format: 3 outputs (YuNet) or 12 outputs (multi-scale)
    if (io_num.n_output == 12) {
        // Multi-scale format: 12 outputs
        // Output[0-2]: conf background for 3 scales (6400, 1600, 400)
        // Output[3-5]: conf face for 3 scales (6400, 1600, 400)
        // Output[6-8]: bbox for 3 scales (6400x4, 1600x4, 400x4)
        // Output[9-11]: landmarks for 3 scales (6400x10, 1600x10, 400x10)
        
        // Get actual counts from output shapes
        std::vector<int> scale_counts;
        std::vector<int> scale_steps = {8, 16, 32};
        std::vector<int> grid_sizes;
        
        static bool logged_scale_info = false;
        for (int scale_idx = 0; scale_idx < 3; scale_idx++) {
            int count = output_attrs[scale_idx + 3].dims[1]; // Face conf output
            scale_counts.push_back(count);
            int grid_size = static_cast<int>(std::sqrt(count));
            if (grid_size * grid_size != count) {
                grid_size = (scale_steps[scale_idx] == 8) ? 80 : (scale_steps[scale_idx] == 16) ? 40 : 20;
            }
            grid_sizes.push_back(grid_size);
            if (!logged_scale_info) {
                std::cout << "DEBUG: Detected multi-scale format (12 outputs)" << std::endl;
                std::cout << "DEBUG: Scale " << scale_idx << ": count=" << count << ", grid=" << grid_size 
                          << ", step=" << scale_steps[scale_idx] << std::endl;
            }
        }
        logged_scale_info = true;
        
        // Pre-calculate transformation parameters
        const float inv_scale = lb_params.scale > 1e-6f ? 1.0f / lb_params.scale : 1.0f;
        const float pad_left_f = static_cast<float>(lb_params.pad_w);
        const float pad_top_f = static_cast<float>(lb_params.pad_h);
        const float padded_w = static_cast<float>(model_input_size);
        const float padded_h = static_cast<float>(model_input_size);
        const std::vector<float> variance = {0.1f, 0.2f};
        
        // Process each scale
        for (int scale_idx = 0; scale_idx < 3; scale_idx++) {
            int num_detections = scale_counts[scale_idx];
            int step = scale_steps[scale_idx];
            int grid_size = grid_sizes[scale_idx];
            
            const float* conf_bg = (const float*)outputs[scale_idx].buf;      // Background conf
            const float* conf_face = (const float*)outputs[scale_idx + 3].buf; // Face conf
            const float* bbox_data = (const float*)outputs[scale_idx + 6].buf; // Bbox [dx, dy, log(w), log(h)]
            const float* landmark_data = (const float*)outputs[scale_idx + 9].buf; // Landmarks
            
            // Statistics for debugging
            int candidates_above_threshold = 0;
            float max_score = 0.0f;
            float min_score = 1e6f;
            int total_checked = 0;
            
            // Process detections for this scale
            for (int i = 0; i < num_detections; i++) {
                float cls_score = conf_face[i];
                float bg_score = conf_bg[i];
                
                // Try different score interpretations
                float score = cls_score;
                
                // Alternative 1: Softmax
                // float exp_face = std::exp(cls_score);
                // float exp_bg = std::exp(bg_score);
                // float score = exp_face / (exp_face + exp_bg);
                
                // Alternative 2: Difference
                // float score = cls_score - bg_score;
                
                // Alternative 3: Sigmoid
                // float score = 1.0f / (1.0f + std::exp(-cls_score));
                
                total_checked++;
                if (score > max_score) max_score = score;
                if (score < min_score) min_score = score;
                
                // Try very low threshold for debugging
                float debug_threshold = 0.1f;  // Very low threshold to see all detections
                
                if (score > debug_threshold) {
                    candidates_above_threshold++;
                    // Decode bbox: [dx, dy, log(w), log(h)]
                    float dx = bbox_data[i * 4 + 0];
                    float dy = bbox_data[i * 4 + 1];
                    float dw = bbox_data[i * 4 + 2];
                    float dh = bbox_data[i * 4 + 3];
                    
                    // Calculate grid position
                    int grid_x = i % grid_size;
                    int grid_y = i / grid_size;
                    
                    // Calculate prior center (normalized [0, 1])
                    float prior_cx = (grid_x + 0.5f) * step / padded_w;
                    float prior_cy = (grid_y + 0.5f) * step / padded_h;
                    
                    // Estimate prior size (normalized)
                    float prior_w = static_cast<float>(step) / padded_w;
                    float prior_h = static_cast<float>(step) / padded_h;
                    
                    // Decode with variance
                    float cx = prior_cx + dx * variance[0] * prior_w;
                    float cy = prior_cy + dy * variance[0] * prior_h;
                    float w_norm = prior_w * std::exp(dw * variance[1]);
                    float h_norm = prior_h * std::exp(dh * variance[1]);
                    
                    // Convert to x1, y1, w, h in padded input space
                    float x1 = (cx - w_norm / 2.0f) * padded_w;
                    float y1 = (cy - h_norm / 2.0f) * padded_h;
                    float w = w_norm * padded_w;
                    float h = h_norm * padded_h;
                    
                    // Transform to original frame space
                    x1 = (x1 - pad_left_f) * inv_scale;
                    y1 = (y1 - pad_top_f) * inv_scale;
                    w = w * inv_scale;
                    h = h * inv_scale;
                    
                    // Convert to integer and clamp (prevent overflow)
                    int x = static_cast<int>(std::round(x1));
                    int y = static_cast<int>(std::round(y1));
                    int width = static_cast<int>(std::round(w));
                    int height = static_cast<int>(std::round(h));
                    
                    // Clamp to reasonable bounds
                    x = std::max(-frame.cols, std::min(x, frame.cols * 2));
                    y = std::max(-frame.rows, std::min(y, frame.rows * 2));
                    width = std::max(1, std::min(width, frame.cols * 2));
                    height = std::max(1, std::min(height, frame.rows * 2));
                    
                    // Final clamp to frame bounds
                    if (x < 0) { width += x; x = 0; }
                    if (y < 0) { height += y; y = 0; }
                    if (x + width > frame.cols) width = frame.cols - x;
                    if (y + height > frame.rows) height = frame.rows - y;
                    
                    if (width > 0 && height > 0 && x >= 0 && y >= 0 && 
                        x < frame.cols && y < frame.rows) {
                        boxes.push_back(cv::Rect(x, y, width, height));
                        scores.push_back(score);
                        
                        // Decode landmarks
                        std::vector<cv::Point2f> lms;
                        for (int j = 0; j < 5; j++) {
                            float kp_dx = landmark_data[i * 10 + j * 2] * variance[0];
                            float kp_dy = landmark_data[i * 10 + j * 2 + 1] * variance[0];
                            
                            float kp_x = (prior_cx + kp_dx * prior_w) * padded_w;
                            float kp_y = (prior_cy + kp_dy * prior_h) * padded_h;
                            
                            kp_x = (kp_x - pad_left_f) * inv_scale;
                            kp_y = (kp_y - pad_top_f) * inv_scale;
                            
                            lms.push_back(cv::Point2f(kp_x, kp_y));
                        }
                        landmarks.push_back(lms);
                    }
                }
            }
            
            // Log statistics for this scale
            static int scale_log_count = 0;
            if (scale_log_count < 3) {
                std::cout << "DEBUG Scale " << scale_idx << ": checked=" << total_checked 
                          << ", max_score=" << max_score << ", min_score=" << min_score
                          << ", candidates_above_0.1=" << candidates_above_threshold << std::endl;
                
                // Sample some confidence values
                if (num_detections > 0) {
                    std::cout << "  Sample conf values (first 10): ";
                    for (int s = 0; s < std::min(10, num_detections); s++) {
                        std::cout << conf_face[s] << " ";
                    }
                    std::cout << std::endl;
                }
                scale_log_count++;
            }
        }
        
        // Log total detections before NMS
        static bool logged_total = false;
        if (!logged_total) {
            std::cout << "DEBUG: Total detections before NMS: " << boxes.size() << std::endl;
            if (!boxes.empty()) {
                std::cout << "DEBUG: Sample detections (first 5):" << std::endl;
                for (size_t i = 0; i < std::min(boxes.size(), size_t(5)); i++) {
                    std::cout << "  [" << i << "] bbox=[" << boxes[i].x << ", " << boxes[i].y 
                              << ", " << boxes[i].width << ", " << boxes[i].height 
                              << "], score=" << scores[i] << std::endl;
                }
            } else {
                std::cout << "WARNING: No detections before NMS! Check:" << std::endl;
                std::cout << "  1. Confidence scores range (see scale debug above)" << std::endl;
                std::cout << "  2. Threshold (currently using 0.1 for debug)" << std::endl;
                std::cout << "  3. Decode logic (bbox coordinates)" << std::endl;
            }
            logged_total = true;
        }
    } else if (io_num.n_output == 3) {
        // YuNet format (3 outputs: loc, conf, iou)
        // YuNet format: decode with priors
        const float* loc_data = (const float*)outputs[0].buf;  // [num_priors, 14] - bbox(4) + landmarks(10)
        const float* conf_data = (const float*)outputs[1].buf; // [num_priors, 2] - bg, face
        const float* iou_data = (const float*)outputs[2].buf;  // [num_priors, 1]
        
        // Get num_priors from output shape
        num_priors = output_attrs[0].dims[0];  // First dimension is num_priors
        
        // Try to determine actual model input size from input attributes
        // Model expects 57600 bytes, which could be 240x240x1 (grayscale) or other format
        int actual_input_size = model_input_size;
        if (input_attrs[0].size == 57600) {
            // Try to infer: 57600 could be 240x240x1 or 240x240 with some format
            // Check if it's square
            if (input_attrs[0].n_dims >= 2) {
                int h = input_attrs[0].dims[input_attrs[0].n_dims - 2];
                int w = input_attrs[0].dims[input_attrs[0].n_dims - 1];
                if (h == w && h > 0) {
                    actual_input_size = h;
                    std::cout << "DEBUG: Inferred actual input size from model: " << actual_input_size << "x" << actual_input_size << std::endl;
                }
            }
            // If can't infer from dims, try: 57600 = 240*240*1
            if (actual_input_size == model_input_size) {
                int test_size = static_cast<int>(std::sqrt(57600));
                if (test_size * test_size == 57600) {
                    actual_input_size = test_size;
                    std::cout << "DEBUG: Inferred input size from bytes: " << actual_input_size << "x" << actual_input_size << std::endl;
                }
            }
        }
        
        // Generate priors for actual input size
        std::vector<PriorBox> priors = generate_priors(actual_input_size, actual_input_size);
        
        if (priors.size() != static_cast<size_t>(num_priors)) {
            std::cerr << "WARNING: Priors count mismatch: generated=" << priors.size() 
                      << " for " << actual_input_size << "x" << actual_input_size
                      << ", expected=" << num_priors << std::endl;
            std::cerr << "WARNING: Will use first " << num_priors << " priors or decode without priors if needed" << std::endl;
            
            // If priors don't match, model might use different format
            // Try to use only the number of priors we have
            if (priors.size() > static_cast<size_t>(num_priors)) {
                priors.resize(num_priors);
            }
        }
        
        // YuNet variance parameters
        const std::vector<float> variance = {0.1f, 0.2f};
        
        // Pre-calculate transformation parameters
        // Use actual input size for decoding, but model_input_size for transformation
        const float padded_w = static_cast<float>(actual_input_size);
        const float padded_h = static_cast<float>(actual_input_size);
        const float inv_scale = lb_params.scale > 1e-6f ? 1.0f / lb_params.scale : 1.0f;
        const float pad_left_f = static_cast<float>(lb_params.pad_w);
        const float pad_top_f = static_cast<float>(lb_params.pad_h);
        
        // Process each detection
        // If priors don't match, try decoding without priors (direct coordinates)
        bool use_priors = (priors.size() == static_cast<size_t>(num_priors));
        
        if (!use_priors) {
            std::cout << "DEBUG: Priors don't match, trying direct coordinate decoding" << std::endl;
        }
        
        for (int i = 0; i < num_priors; i++) {
            // Get confidence scores
            float cls_score = conf_data[i * 2 + 1];  // Face class score
            float iou_score_raw = iou_data[i];
            
            // Normalize IOU score (clamp to [0, 1] or use scale if > 1)
            float iou_score = iou_score_raw;
            if (iou_score_raw > 1.0f) {
                // Try scaling: divide by 2.0 if max is around 2.0
                iou_score = std::min(1.0f, iou_score_raw / 2.0f);
            } else {
                iou_score = std::max(0.0f, std::min(1.0f, iou_score_raw));
            }
            
            // Combined score: sqrt(clsScore * iouScore) - matching OpenCV FaceDetectorYN
            float score = std::sqrt(cls_score * iou_score);
            
            if (score > CONF_THRESHOLD) {
                const float* loc_ptr = loc_data + i * 14;
                
                float x1, y1, w, h;
                
                if (use_priors && i < static_cast<int>(priors.size())) {
                    // Decode with priors (standard YuNet)
                    const PriorBox& prior = priors[i];
                    
                    // Decode bbox: [dx, dy, log(w), log(h)] with variance
                    float dx = loc_ptr[0] * variance[0];
                    float dy = loc_ptr[1] * variance[0];
                    float dw = loc_ptr[2] * variance[1];
                    float dh = loc_ptr[3] * variance[1];
                    
                    // Decode center and size
                    float cx = prior.cx + dx * prior.w;
                    float cy = prior.cy + dy * prior.h;
                    float w_norm = prior.w * std::exp(dw);
                    float h_norm = prior.h * std::exp(dh);
                    
                    // Convert to x1, y1, w, h in padded input space
                    x1 = (cx - w_norm / 2.0f) * padded_w;
                    y1 = (cy - h_norm / 2.0f) * padded_h;
                    w = w_norm * padded_w;
                    h = h_norm * padded_h;
                } else {
                    // Try direct coordinate interpretation
                    // Check if coordinates are normalized [0, 1] or absolute
                    float test_x = loc_ptr[0];
                    float test_y = loc_ptr[1];
                    bool is_normalized = (test_x >= 0.0f && test_x <= 1.0f && 
                                         test_y >= 0.0f && test_y <= 1.0f);
                    
                    if (is_normalized) {
                        // Normalized coordinates [0, 1]
                        x1 = loc_ptr[0] * padded_w;
                        y1 = loc_ptr[1] * padded_h;
                        w = loc_ptr[2] * padded_w;
                        h = loc_ptr[3] * padded_h;
                    } else {
                        // Absolute coordinates or offsets
                        x1 = loc_ptr[0];
                        y1 = loc_ptr[1];
                        w = loc_ptr[2];
                        h = loc_ptr[3];
                        
                        // If w/h are very small, might be log scale
                        if (w > 0.0f && w < 1.0f && h > 0.0f && h < 1.0f) {
                            w = std::exp(w) * padded_w;
                            h = std::exp(h) * padded_h;
                        }
                    }
                }
                
                // Transform from padded input space to original frame space
                // Step 1: Remove padding
                x1 = x1 - pad_left_f;
                y1 = y1 - pad_top_f;
                
                // Step 2: Scale back to original frame size
                x1 = x1 * inv_scale;
                y1 = y1 * inv_scale;
                w = w * inv_scale;
                h = h * inv_scale;
                
                // Convert to integer and clamp to frame bounds
                int x = static_cast<int>(std::round(x1));
                int y = static_cast<int>(std::round(y1));
                int width = static_cast<int>(std::round(w));
                int height = static_cast<int>(std::round(h));
                
                x = std::max(0, std::min(x, frame.cols - 1));
                y = std::max(0, std::min(y, frame.rows - 1));
                width = std::max(1, std::min(width, frame.cols - x));
                height = std::max(1, std::min(height, frame.rows - y));
                
                boxes.push_back(cv::Rect(x, y, width, height));
                scores.push_back(score);
                
                // Decode landmarks (5 keypoints)
                std::vector<cv::Point2f> lms;
                for (int j = 0; j < 5; j++) {
                    float kp_x, kp_y;
                    
                    if (use_priors && i < static_cast<int>(priors.size())) {
                        // Decode with priors
                        const PriorBox& prior = priors[i];
                        float kp_dx = loc_ptr[4 + j * 2] * variance[0];
                        float kp_dy = loc_ptr[5 + j * 2] * variance[0];
                        
                        kp_x = (prior.cx + kp_dx * prior.w) * padded_w;
                        kp_y = (prior.cy + kp_dy * prior.h) * padded_h;
                    } else {
                        // Direct coordinates
                        kp_x = loc_ptr[4 + j * 2];
                        kp_y = loc_ptr[5 + j * 2];
                        
                        // Check if normalized
                        if (kp_x >= 0.0f && kp_x <= 1.0f && kp_y >= 0.0f && kp_y <= 1.0f) {
                            kp_x = kp_x * padded_w;
                            kp_y = kp_y * padded_h;
                        }
                    }
                    
                    // Transform to original frame space
                    kp_x = (kp_x - pad_left_f) * inv_scale;
                    kp_y = (kp_y - pad_top_f) * inv_scale;
                    
                    lms.push_back(cv::Point2f(kp_x, kp_y));
                }
                landmarks.push_back(lms);
            }
        }
    } else {
        // Fallback: try old format
        float* score_data = (float*)outputs[0].buf;
        float* box_data = (float*)outputs[1].buf;
        float* landmark_data = (float*)outputs[2].buf;
        
        int num_detections = num_priors;
        if (output_attrs[0].n_dims >= 2) {
            num_detections = output_attrs[0].dims[1];
        }
        
        for (int i = 0; i < num_detections; i++) {
            float conf = score_data[i * 2 + 1];
            
            if (conf > CONF_THRESHOLD) {
                float x = box_data[i * 4 + 0];
                float y = box_data[i * 4 + 1];
                float w = box_data[i * 4 + 2];
                float h = box_data[i * 4 + 3];
                
                x = (x - lb_params.pad_w) / lb_params.scale;
                y = (y - lb_params.pad_h) / lb_params.scale;
                w = w / lb_params.scale;
                h = h / lb_params.scale;
                
                boxes.push_back(cv::Rect(static_cast<int>(x), static_cast<int>(y), 
                                        static_cast<int>(w), static_cast<int>(h)));
                scores.push_back(conf);
                
                std::vector<cv::Point2f> lms;
                for (int j = 0; j < 5; j++) {
                    float lm_x = landmark_data[i * 10 + j * 2 + 0];
                    float lm_y = landmark_data[i * 10 + j * 2 + 1];
                    
                    lm_x = (lm_x - lb_params.pad_w) / lb_params.scale;
                    lm_y = (lm_y - lb_params.pad_h) / lb_params.scale;
                    
                    lms.push_back(cv::Point2f(lm_x, lm_y));
                }
                landmarks.push_back(lms);
            }
        }
    }
    
    // Log before NMS
    static bool logged_before_nms = false;
    if (!logged_before_nms && !boxes.empty()) {
        std::cout << "DEBUG: Before NMS: " << boxes.size() << " detections" << std::endl;
        for (size_t i = 0; i < std::min(boxes.size(), size_t(5)); i++) {
            std::cout << "  Detection " << i << ": bbox=[" << boxes[i].x << ", " << boxes[i].y 
                      << ", " << boxes[i].width << ", " << boxes[i].height 
                      << "], score=" << scores[i] << std::endl;
        }
        logged_before_nms = true;
    }
    
    // Apply NMS
    nms(boxes, scores, landmarks, NMS_THRESHOLD);
    
    // Log after NMS
    static bool logged_after_nms = false;
    if (!logged_after_nms) {
        std::cout << "DEBUG: After NMS: " << boxes.size() << " detections" << std::endl;
        if (!boxes.empty()) {
            for (size_t i = 0; i < std::min(boxes.size(), size_t(5)); i++) {
                std::cout << "  Final " << i << ": bbox=[" << boxes[i].x << ", " << boxes[i].y 
                          << ", " << boxes[i].width << ", " << boxes[i].height 
                          << "], score=" << scores[i] << std::endl;
            }
        } else {
            std::cout << "WARNING: No detections after NMS! Check threshold and decode logic." << std::endl;
        }
        logged_after_nms = true;
    }
    
    // Create FaceDetection results
    std::vector<FaceDetection> detections;
    for (size_t i = 0; i < boxes.size(); i++) {
        FaceDetection det;
        det.bbox = boxes[i];
        det.score = scores[i];
        det.landmarks = landmarks[i];
        detections.push_back(det);
    }
    
    // Create output directory for cropped faces
    static bool dir_created = false;
    static std::string crop_dir = "debug_cropped_faces";
    if (!dir_created) {
        mkdir(crop_dir.c_str(), 0755);
        dir_created = true;
    }
    
    // Draw results and crop faces
    for (size_t i = 0; i < detections.size(); i++) {
        const auto& det = detections[i];
        
        // Draw bounding box on original frame
        cv::rectangle(frame, det.bbox, cv::Scalar(0, 255, 0), 2);
        
        std::string label = cv::format("%.2f", det.score);
        cv::putText(frame, label, cv::Point(det.bbox.x, det.bbox.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        
        // Draw landmarks on original frame
        for (const auto& lm : det.landmarks) {
            cv::circle(frame, lm, 2, cv::Scalar(0, 0, 255), -1);
        }
        
        // Crop face from original frame
        cv::Rect crop_rect = det.bbox;
        // Add padding (10% on each side)
        int pad_x = std::max(0, static_cast<int>(crop_rect.width * 0.1f));
        int pad_y = std::max(0, static_cast<int>(crop_rect.height * 0.1f));
        crop_rect.x = std::max(0, crop_rect.x - pad_x);
        crop_rect.y = std::max(0, crop_rect.y - pad_y);
        crop_rect.width = std::min(frame.cols - crop_rect.x, crop_rect.width + 2 * pad_x);
        crop_rect.height = std::min(frame.rows - crop_rect.y, crop_rect.height + 2 * pad_y);
        
        if (crop_rect.width > 0 && crop_rect.height > 0 && 
            crop_rect.x >= 0 && crop_rect.y >= 0 &&
            crop_rect.x + crop_rect.width <= frame.cols &&
            crop_rect.y + crop_rect.height <= frame.rows) {
            
            cv::Mat cropped_face = frame(crop_rect).clone();
            
            // Draw bounding box and landmarks on cropped face
            // Adjust coordinates to cropped face space
            cv::Rect bbox_in_crop = det.bbox;
            bbox_in_crop.x -= crop_rect.x;
            bbox_in_crop.y -= crop_rect.y;
            
            if (bbox_in_crop.x >= 0 && bbox_in_crop.y >= 0 &&
                bbox_in_crop.x + bbox_in_crop.width <= cropped_face.cols &&
                bbox_in_crop.y + bbox_in_crop.height <= cropped_face.rows) {
                
                cv::rectangle(cropped_face, bbox_in_crop, cv::Scalar(0, 255, 0), 2);
                
                // Draw landmarks on cropped face
                for (const auto& lm : det.landmarks) {
                    cv::Point2f lm_in_crop(lm.x - crop_rect.x, lm.y - crop_rect.y);
                    if (lm_in_crop.x >= 0 && lm_in_crop.x < cropped_face.cols &&
                        lm_in_crop.y >= 0 && lm_in_crop.y < cropped_face.rows) {
                        cv::circle(cropped_face, lm_in_crop, 2, cv::Scalar(0, 0, 255), -1);
                    }
                }
                
                // Add score label
                cv::putText(cropped_face, label, cv::Point(bbox_in_crop.x, bbox_in_crop.y - 5),
                           cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
            }
            
            // Save cropped face
            static int face_counter = 0;
            std::string crop_filename = crop_dir + "/face_" + std::to_string(face_counter++) + 
                                       "_score" + cv::format("%.3f", det.score) + ".jpg";
            cv::imwrite(crop_filename, cropped_face);
        }
    }
    
    // Draw face count on frame
    std::string face_count_text = "Faces: " + std::to_string(detections.size());
    cv::putText(frame, face_count_text, cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 0), 2);
    
    // Release outputs
    rknn_outputs_release(ctx, io_num.n_output, outputs);
    
    return detections;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cout << "Usage: " << argv[0] << " <image_or_video_path> [model_path]" << std::endl;
        std::cout << "  If model_path is not provided, will try to find model in:" << std::endl;
        std::cout << "    - ./face_detection_yunet_2023mar.rknn" << std::endl;
        std::cout << "    - ./yunet/face_detection_yunet_2023mar.rknn" << std::endl;
        std::cout << "    - ./face_detection_yunet_2022mar_int8.rknn" << std::endl;
        std::cout << "    - ../cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn" << std::endl;
        std::cout << "    - ../cvedix_data/models/face/rk3588/face_detection_yunet_2023mar.rknn" << std::endl;
        return -1;
    }
    
    std::string input_path = argv[1];
    
    // Determine model path
    std::string model_path;
    if (argc == 3) {
        model_path = argv[2];
    } else {
        // Try multiple possible paths (prioritize 2023mar model)
        std::vector<std::string> possible_paths = {
            "./face_detection_yunet_2023mar.rknn",
            "./yunet/face_detection_yunet_2023mar.rknn",
            "test/yunet/face_detection_yunet_2023mar.rknn",
            "../cvedix_data/models/face/rk3588/face_detection_yunet_2023mar.rknn",
            "../cvedix_data/models/face/rk3588/face_detection_yunet_2023mar_sim_fp.rknn",
            "./face_detection_yunet_2022mar_int8.rknn",
            "./yunet/face_detection_yunet_2022mar_int8.rknn",
            "test/yunet/face_detection_yunet_2022mar_int8.rknn"
        };
        
        bool found = false;
        for (const auto& path : possible_paths) {
            std::ifstream test_file(path, std::ios::binary);
            if (test_file.good()) {
                model_path = path;
                found = true;
                std::cout << "Found model at: " << model_path << std::endl;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "ERROR: Model file not found. Tried the following paths:" << std::endl;
            for (const auto& path : possible_paths) {
                std::cerr << "  - " << path << std::endl;
            }
            std::cerr << "\nPlease provide model path as second argument:" << std::endl;
            std::cerr << "  " << argv[0] << " <image_or_video_path> <model_path>" << std::endl;
            return -1;
        }
    }
    
    // Load RKNN model
    std::ifstream model_file(model_path, std::ios::binary | std::ios::ate);
    if (!model_file.is_open()) {
        std::cerr << "ERROR: Failed to open model file: " << model_path << std::endl;
        std::cerr << "  Current working directory: ";
        char* cwd = getcwd(nullptr, 0);
        if (cwd) {
            std::cerr << cwd << std::endl;
            free(cwd);
        } else {
            std::cerr << "unknown" << std::endl;
        }
        std::cerr << "  File exists check: ";
        std::ifstream test(model_path);
        if (test.good()) {
            std::cerr << "YES (but cannot open for reading)" << std::endl;
        } else {
            std::cerr << "NO" << std::endl;
        }
        std::cerr << "  Error details: " << strerror(errno) << std::endl;
        return -1;
    }
    
    size_t model_size = model_file.tellg();
    model_file.seekg(0, std::ios::beg);
    
    std::vector<char> model_data(model_size);
    model_file.read(model_data.data(), model_size);
    model_file.close();
    
    std::cout << "Model loaded: " << model_size << " bytes" << std::endl;
    
    // Initialize RKNN
    rknn_context ctx;
    int ret = rknn_init(&ctx, model_data.data(), model_size, 0, nullptr);
    if (ret != RKNN_SUCC) {
        std::cerr << "rknn_init failed: " << ret << std::endl;
        return -1;
    }
    
    std::cout << "RKNN initialized successfully" << std::endl;
    
    // Query model input/output info
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        std::cerr << "rknn_query failed: " << ret << std::endl;
        rknn_destroy(ctx);
        return -1;
    }
    
    std::cout << "Model has " << io_num.n_input << " inputs and " << io_num.n_output << " outputs" << std::endl;
    
    // Get input attributes
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (uint32_t i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            std::cerr << "rknn_query input attr failed: " << ret << std::endl;
            rknn_destroy(ctx);
            return -1;
        }
        
        // Log input attributes
        std::cout << "Input[" << i << "]: name=" << input_attrs[i].name 
                  << ", n_dims=" << input_attrs[i].n_dims
                  << ", dims=[";
        for (uint32_t d = 0; d < input_attrs[i].n_dims; d++) {
            if (d > 0) std::cout << ",";
            std::cout << input_attrs[i].dims[d];
        }
        std::cout << "], type=" << input_attrs[i].type 
                  << ", fmt=" << input_attrs[i].fmt
                  << ", size=" << input_attrs[i].size << " bytes" << std::endl;
        
        // Determine input size from attributes
        if (input_attrs[i].n_dims >= 2) {
            int width = 0, height = 0;
            if (input_attrs[i].fmt == RKNN_TENSOR_NCHW) {
                height = input_attrs[i].dims[2];
                width = input_attrs[i].dims[3];
            } else if (input_attrs[i].fmt == RKNN_TENSOR_NHWC) {
                height = input_attrs[i].dims[1];
                width = input_attrs[i].dims[2];
            } else {
                height = input_attrs[i].dims[input_attrs[i].n_dims - 2];
                width = input_attrs[i].dims[input_attrs[i].n_dims - 1];
            }
            if (width > 0 && height > 0 && width == height) {
                INPUT_SIZE = width;
                std::cout << "Detected model input size: " << INPUT_SIZE << "x" << INPUT_SIZE << std::endl;
            }
        }
    }
    
    // Get output attributes
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    for (uint32_t i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            std::cerr << "rknn_query output attr failed: " << ret << std::endl;
            rknn_destroy(ctx);
            return -1;
        }
        
        // Log output attributes
        std::cout << "Output[" << i << "]: name=" << output_attrs[i].name 
                  << ", n_dims=" << output_attrs[i].n_dims
                  << ", dims=[";
        for (uint32_t d = 0; d < output_attrs[i].n_dims; d++) {
            if (d > 0) std::cout << ",";
            std::cout << output_attrs[i].dims[d];
        }
        std::cout << "], type=" << output_attrs[i].type 
                  << ", fmt=" << output_attrs[i].fmt
                  << ", size=" << output_attrs[i].size << " bytes" << std::endl;
    }
    
    // Try to open as video first
    cv::VideoCapture cap(input_path);
    bool is_video = cap.isOpened();
    
    if (is_video) {
        std::cout << "Processing video: " << input_path << std::endl;
        
        int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        double fps = cap.get(cv::CAP_PROP_FPS);
        int total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        
        std::cout << "Video info: " << frame_width << "x" << frame_height 
                  << " @ " << fps << " FPS, " << total_frames << " frames" << std::endl;
        
        // Create video writer
        cv::VideoWriter writer("output_rknn.mp4", 
                              cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                              fps, cv::Size(frame_width, frame_height));
        
        if (!writer.isOpened()) {
            std::cerr << "Failed to create video writer" << std::endl;
            rknn_destroy(ctx);
            return -1;
        }
        
        cv::Mat frame;
        int frame_count = 0;
        int total_faces_detected = 0;
        int frames_with_faces = 0;
        
        while (cap.read(frame)) {
            frame_count++;
            
            // Process frame and get detected faces
            std::vector<FaceDetection> detections = process_frame(frame, ctx, io_num, input_attrs, output_attrs, INPUT_SIZE);
            int num_faces = static_cast<int>(detections.size());
            total_faces_detected += num_faces;
            if (num_faces > 0) {
                frames_with_faces++;
            }
            
            // Log every 10 frames or when faces are detected
            if (frame_count % 10 == 0 || num_faces > 0) {
                std::cout << "[Frame " << frame_count << "/" << total_frames << "] "
                          << "Faces detected: " << num_faces << std::endl;
                // Print bounding boxes
                for (size_t i = 0; i < detections.size(); i++) {
                    const auto& det = detections[i];
                    std::cout << "  Face " << (i+1) << ": bbox=[" << det.bbox.x << ", " << det.bbox.y 
                              << ", " << det.bbox.width << ", " << det.bbox.height 
                              << "], score=" << std::fixed << std::setprecision(3) << det.score << std::endl;
                }
            }
            
            writer.write(frame);
        }
        
        // Print summary statistics
        std::cout << "\n=== Processing Summary ===" << std::endl;
        std::cout << "Total frames processed: " << frame_count << std::endl;
        std::cout << "Total faces detected: " << total_faces_detected << std::endl;
        std::cout << "Frames with faces: " << frames_with_faces << std::endl;
        if (frame_count > 0) {
            std::cout << "Average faces per frame: " 
                      << (static_cast<float>(total_faces_detected) / frame_count) << std::endl;
        }
        
        cap.release();
        writer.release();
        
        std::cout << "Video processing completed. Output saved to: output_rknn.mp4" << std::endl;
        
    } else {
        // Process as image
        cv::Mat img = cv::imread(input_path);
        if (img.empty()) {
            std::cerr << "Failed to load image: " << input_path << std::endl;
            rknn_destroy(ctx);
            return -1;
        }
        
        std::cout << "Processing image: " << img.cols << "x" << img.rows << std::endl;
        
        // Process image and get detected faces
        std::vector<FaceDetection> detections = process_frame(img, ctx, io_num, input_attrs, output_attrs, INPUT_SIZE);
        int num_faces = static_cast<int>(detections.size());
        
        // Log detection results
        std::cout << "\n=== Detection Results ===" << std::endl;
        std::cout << "Faces detected: " << num_faces << std::endl;
        
        // Print bounding boxes
        for (size_t i = 0; i < detections.size(); i++) {
            const auto& det = detections[i];
            std::cout << "\nFace " << (i+1) << ":" << std::endl;
            std::cout << "  Bounding Box: [" << det.bbox.x << ", " << det.bbox.y 
                      << ", " << det.bbox.width << ", " << det.bbox.height << "]" << std::endl;
            std::cout << "  Score: " << std::fixed << std::setprecision(3) << det.score << std::endl;
            std::cout << "  Landmarks: ";
            for (size_t j = 0; j < det.landmarks.size(); j++) {
                if (j > 0) std::cout << ", ";
                std::cout << "(" << det.landmarks[j].x << ", " << det.landmarks[j].y << ")";
            }
            std::cout << std::endl;
        }
        
        // Save result
        std::string output_path = "output_rknn.jpg";
        cv::imwrite(output_path, img);
        std::cout << "Result saved to: " << output_path << std::endl;
    }
    
    // Destroy RKNN context
    rknn_destroy(ctx);
    
    return 0;
}
