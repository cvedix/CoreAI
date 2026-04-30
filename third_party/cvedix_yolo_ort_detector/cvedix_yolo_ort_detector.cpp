/**
 * @file cvedix_yolo_ort_detector.cpp
 * @brief ONNX Runtime YOLO detector implementation
 */

#include "cvedix_yolo_ort_detector.h"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace cvedix_yolo_ort {

// ──────────────────────── Constructor ──────────────────
cvedix_yolo_ort_detector::cvedix_yolo_ort_detector(
    const std::string& onnx_path,
    float conf_threshold,
    float nms_threshold,
    int num_classes)
    : conf_threshold_(conf_threshold)
    , nms_threshold_(nms_threshold)
    , num_classes_(num_classes)
    , mem_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
{
    if (!load_model(onnx_path)) {
        throw std::runtime_error("Failed to load ONNX model: " + onnx_path);
    }

    std::cout << "[cvedix_yolo_ort] Loaded ONNX model: " << onnx_path << std::endl;
    std::cout << "[cvedix_yolo_ort] Input: " << input_width_ << "x" << input_height_
              << ", Classes: " << num_classes_ << std::endl;
}

// ──────────────────────── Destructor ──────────────────
cvedix_yolo_ort_detector::~cvedix_yolo_ort_detector() = default;

// ──────────────────────── load_model ──────────────────
bool cvedix_yolo_ort_detector::load_model(const std::string& onnx_path) {
    std::ifstream file(onnx_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[cvedix_yolo_ort] Model file not found: " << onnx_path << std::endl;
        return false;
    }
    file.close();

    try {
        // Initialize ORT environment
        env_ = Ort::Env(ORT_LOGGING_LEVEL_WARNING, "cvedix_yolo_ort_detector");

        // Configure session options
        session_options_ = std::make_unique<Ort::SessionOptions>();
        session_options_->SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);
        session_options_->SetIntraOpNumThreads(4);
        session_options_->SetInterOpNumThreads(4);

        // Create session
        session_ = std::make_unique<Ort::Session>(env_, onnx_path.c_str(), *session_options_);

        // Get input info
        size_t num_input_nodes = session_->GetInputCount();
        if (num_input_nodes == 0) {
            std::cerr << "[cvedix_yolo_ort] No input nodes found" << std::endl;
            return false;
        }

        auto input_name_ptr = session_->GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        input_names_.push_back(input_name_ptr.get());

        Ort::TypeInfo input_info = session_->GetInputTypeInfo(0);
        auto input_shape_info = input_info.GetTensorTypeAndShapeInfo();
        input_shape_ = input_shape_info.GetShape();
        input_name_ptr.release();

        // Determine input size from shape
        if (input_shape_.size() >= 4) {
            // Typical: [batch, channels, height, width]
            if (input_shape_[2] > 0) input_height_ = static_cast<int>(input_shape_[2]);
            if (input_shape_[3] > 0) input_width_ = static_cast<int>(input_shape_[3]);
        } else if (input_shape_.size() >= 3) {
            if (input_shape_[1] > 0) input_height_ = static_cast<int>(input_shape_[1]);
            if (input_shape_[2] > 0) input_width_ = static_cast<int>(input_shape_[2]);
        }

        std::cout << "[cvedix_yolo_ort] Input shape: "
                  << input_shape_[0] << "x" << input_shape_[1] << "x"
                  << input_shape_[2] << "x" << input_shape_[3] << std::endl;

        // Get output info
        size_t num_output_nodes = session_->GetOutputCount();
        if (num_output_nodes == 0) {
            std::cerr << "[cvedix_yolo_ort] No output nodes found" << std::endl;
            return false;
        }

        auto output_name_ptr = session_->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        output_names_.push_back(output_name_ptr.get());
        output_name_ptr.release();

        Ort::TypeInfo output_info = session_->GetOutputTypeInfo(0);
        auto output_shape_info = output_info.GetTensorTypeAndShapeInfo();
        auto output_shape = output_shape_info.GetShape();

        std::cout << "[cvedix_yolo_ort] Output shape: ";
        for (size_t i = 0; i < output_shape.size(); i++) {
            std::cout << output_shape[i];
            if (i < output_shape.size() - 1) std::cout << "x";
        }
        std::cout << std::endl;

        // Analyze output shape to determine format
        analyze_output_shape();

        std::cout << "[cvedix_yolo_ort] Output format: "
                  << (output_transposed_ ? "transposed [1,84,N]" : "standard [1,N,84]")
                  << ", anchors: " << num_anchors_ << std::endl;

        return true;
    }
    catch (const Ort::Exception& e) {
        std::cerr << "[cvedix_yolo_ort] ONNX Runtime Error: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception& e) {
        std::cerr << "[cvedix_yolo_ort] Error: " << e.what() << std::endl;
        return false;
    }
}

// ──────────────────────── analyze_output_shape ─────────
void cvedix_yolo_ort_detector::analyze_output_shape() {
    // YOLOv11 anchor-free output formats:
    // [1, 4+NC, 8400]  → transposed, where NC = num_classes (no obj conf in v11)
    // [1, 8400, 4+NC]  → standard
    //
    // Examples:
    //   [1, 84, 8400]  → 80-class (COCO): 4 bbox + 80 classes, transposed
    //   [1, 5, 8400]   → 1-class (face):  4 bbox + 1 class, transposed
    //   [1, 85, 8400]  → legacy YOLOv5/v8: 4 bbox + 1 objectness + 80 classes

    Ort::TypeInfo output_info = session_->GetOutputTypeInfo(0);
    auto output_shape_info = output_info.GetTensorTypeAndShapeInfo();
    auto output_shape = output_shape_info.GetShape();

    int dim0 = 0, dim1 = 0;

    if (output_shape.size() == 3) {
        dim0 = static_cast<int>(output_shape[1]);
        dim1 = static_cast<int>(output_shape[2]);
    } else if (output_shape.size() == 2) {
        dim0 = static_cast<int>(output_shape[0]);
        dim1 = static_cast<int>(output_shape[1]);
    }

    // Heuristic: the smaller dimension is the "features" dim (4+NC),
    // the larger dimension is the "anchors" dim (e.g. 8400)
    int feat_dim, anchor_dim;
    bool transposed;

    if (dim0 < dim1) {
        // [B, feat, anchors] → transposed
        feat_dim = dim0;
        anchor_dim = dim1;
        transposed = true;
    } else {
        // [B, anchors, feat] → standard
        feat_dim = dim1;
        anchor_dim = dim0;
        transposed = false;
    }

    // YOLOv11 anchor-free: feat_dim = 4 + num_classes (no objectness)
    // YOLOv5/v8 legacy:   feat_dim = 5 + num_classes (with objectness)
    int nc_v11 = feat_dim - 4;  // YOLOv11 interpretation
    int nc_v5  = feat_dim - 5;  // YOLOv5/v8 interpretation

    if (nc_v11 > 0 && nc_v11 <= 1000) {
        // Prefer YOLOv11 format (no separate objectness conf)
        num_classes_ = nc_v11;
        has_objectness_ = false;
    } else if (nc_v5 > 0 && nc_v5 <= 1000) {
        // Legacy format with objectness
        num_classes_ = nc_v5;
        has_objectness_ = true;
    } else {
        // Extreme fallback
        num_classes_ = 80;
        has_objectness_ = false;
    }

    num_anchors_ = anchor_dim;
    output_transposed_ = transposed;

    if (num_anchors_ <= 0) num_anchors_ = 8400;

    std::cout << "[cvedix_yolo_ort] Detected: "
              << num_classes_ << " classes, "
              << num_anchors_ << " anchors, "
              << (has_objectness_ ? "with" : "without") << " objectness, "
              << (output_transposed_ ? "transposed" : "standard") << std::endl;
}

// ──────────────────────── preprocess ──────────────────
void cvedix_yolo_ort_detector::preprocess(const cv::Mat& image, std::vector<float>& blob) {
    int orig_w = image.cols;
    int orig_h = image.rows;

    float scale = std::min(
        static_cast<float>(input_width_) / orig_w,
        static_cast<float>(input_height_) / orig_h
    );

    int new_w = static_cast<int>(round(orig_w * scale));
    int new_h = static_cast<int>(round(orig_h * scale));

    int pad_w = (input_width_ - new_w) / 2;
    int pad_h = (input_height_ - new_h) / 2;

    letterbox_scale_ = scale;
    letterbox_pad_x_ = static_cast<float>(pad_w);
    letterbox_pad_y_ = static_cast<float>(pad_h);

    // Resize maintaining aspect ratio
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_w, new_h));

    // Pad to input size (114 = standard YOLO padding)
    cv::Mat padded(input_height_, input_width_, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(pad_w, pad_h, new_w, new_h)));

    // Convert to float blob: [1, 3, H, W] with BGR→RGB and normalization
    blob.resize(1 * 3 * input_height_ * input_width_);
    float* ptr = blob.data();

    for (int c = 0; c < 3; c++) {
        for (int h = 0; h < input_height_; h++) {
            for (int w = 0; w < input_width_; w++) {
                cv::Vec3b pixel = padded.at<cv::Vec3b>(h, w);
                // BGR → RGB (YOLO expects RGB)
                int src_channel = (c == 0) ? 2 : (c == 2) ? 0 : 1;
                ptr[c * input_height_ * input_width_ + h * input_width_ + w] =
                    static_cast<float>(pixel[src_channel]) / 255.0f;
            }
        }
    }
}

// ──────────────────────── postprocess ──────────────────
void cvedix_yolo_ort_detector::postprocess(
    std::vector<float>& output_data,
    const std::vector<int64_t>& output_shape,
    const cv::Size& original_size,
    std::vector<Detection>& detections) {

    detections.clear();

    int num_anchors = num_anchors_;
    // YOLOv11: stride = 4 + num_classes (no objectness)
    // Legacy:  stride = 4 + 1 + num_classes (with objectness)
    int stride = has_objectness_ ? (4 + 1 + num_classes_) : (4 + num_classes_);

    float* data = output_data.data();
    int total_elements = static_cast<int>(output_data.size());

    // Safety check
    if (num_anchors * stride > total_elements) {
        std::cerr << "[cvedix_yolo_ort] Output buffer too small: "
                  << total_elements << " < " << num_anchors * stride << std::endl;
        return;
    }

    for (int a = 0; a < num_anchors; a++) {
        float* anchor_data;

        if (output_transposed_) {
            // Shape: [1, stride, num_anchors] → column-major per anchor
            // anchor_data[i] = data[i * num_anchors + a]
            // We need to gather data for this anchor across stride dimension
        } else {
            // Shape: [1, num_anchors, stride] → row-major per anchor
            anchor_data = &data[a * stride];
        }

        // For transposed layout, gather values manually
        float cx, cy, w, h;
        float best_class_score = 0.0f;
        int best_class = 0;

        if (output_transposed_) {
            // [1, stride, num_anchors]: value at [0, dim, anchor] = data[dim * num_anchors + anchor]
            cx = data[0 * num_anchors + a];
            cy = data[1 * num_anchors + a];
            w  = data[2 * num_anchors + a];
            h  = data[3 * num_anchors + a];

            int class_offset = has_objectness_ ? 5 : 4;
            float obj_conf = 1.0f;
            if (has_objectness_) {
                obj_conf = sigmoid(data[4 * num_anchors + a]);
            }

            for (int c = 0; c < num_classes_; c++) {
                float cls_score = data[(class_offset + c) * num_anchors + a];
                // YOLOv11 uses raw scores, apply sigmoid for probability
                float score = has_objectness_ ? (obj_conf * sigmoid(cls_score)) : cls_score;
                if (score > best_class_score) {
                    best_class_score = score;
                    best_class = c;
                }
            }
        } else {
            anchor_data = &data[a * stride];
            cx = anchor_data[0];
            cy = anchor_data[1];
            w  = anchor_data[2];
            h  = anchor_data[3];

            int class_offset = has_objectness_ ? 5 : 4;
            float obj_conf = 1.0f;
            if (has_objectness_) {
                obj_conf = sigmoid(anchor_data[4]);
            }

            for (int c = 0; c < num_classes_; c++) {
                float cls_score = anchor_data[class_offset + c];
                float score = has_objectness_ ? (obj_conf * sigmoid(cls_score)) : cls_score;
                if (score > best_class_score) {
                    best_class_score = score;
                    best_class = c;
                }
            }
        }

        if (best_class_score < conf_threshold_) continue;

        // YOLOv11 outputs raw pixel coordinates in input (letterboxed) space
        // Convert from letterbox space to original image space
        float orig_cx = (cx - letterbox_pad_x_) / letterbox_scale_;
        float orig_cy = (cy - letterbox_pad_y_) / letterbox_scale_;
        float orig_w = w / letterbox_scale_;
        float orig_h = h / letterbox_scale_;

        // Clamp
        orig_cx = std::max(0.0f, std::min(orig_cx, static_cast<float>(original_size.width)));
        orig_cy = std::max(0.0f, std::min(orig_cy, static_cast<float>(original_size.height)));
        orig_w = std::max(0.0f, std::min(orig_w, static_cast<float>(original_size.width)));
        orig_h = std::max(0.0f, std::min(orig_h, static_cast<float>(original_size.height)));

        Detection det;
        det.bbox[0] = orig_cx;
        det.bbox[1] = orig_cy;
        det.bbox[2] = orig_w;
        det.bbox[3] = orig_h;
        det.conf = best_class_score;
        det.class_id = best_class;

        detections.push_back(det);
    }

    apply_nms(detections);
}

// ──────────────────────── apply_nms ──────────────────
void cvedix_yolo_ort_detector::apply_nms(std::vector<Detection>& detections) {
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

            // Calculate IoU
            float ax1 = detections[i].bbox[0] - detections[i].bbox[2] * 0.5f;
            float ay1 = detections[i].bbox[1] - detections[i].bbox[3] * 0.5f;
            float ax2 = detections[i].bbox[0] + detections[i].bbox[2] * 0.5f;
            float ay2 = detections[i].bbox[1] + detections[i].bbox[3] * 0.5f;

            float bx1 = detections[j].bbox[0] - detections[j].bbox[2] * 0.5f;
            float by1 = detections[j].bbox[1] - detections[j].bbox[3] * 0.5f;
            float bx2 = detections[j].bbox[0] + detections[j].bbox[2] * 0.5f;
            float by2 = detections[j].bbox[1] + detections[j].bbox[3] * 0.5f;

            float inter_xmin = std::max(ax1, bx1);
            float inter_ymin = std::max(ay1, by1);
            float inter_xmax = std::min(ax2, bx2);
            float inter_ymax = std::min(ay2, by2);

            if (inter_xmin < inter_xmax && inter_ymin < inter_ymax) {
                float inter_area = (inter_xmax - inter_xmin) * (inter_ymax - inter_ymin);
                float area1 = detections[i].bbox[2] * detections[i].bbox[3];
                float area2 = detections[j].bbox[2] * detections[j].bbox[3];
                float union_area = area1 + area2 - inter_area;
                float iou = inter_area / union_area;

                if (iou > nms_threshold_) {
                    suppressed[j] = true;
                }
            }
        }
    }

    detections = std::move(result);
}

// ──────────────────────── detect (single) ──────────────
void cvedix_yolo_ort_detector::detect(const cv::Mat& image,
                                       std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = batch_detections[0];
    }
}

// ──────────────────────── detect (batch) ──────────────
void cvedix_yolo_ort_detector::detect(const std::vector<cv::Mat>& images,
                                       std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());

    if (images.empty()) return;

    Ort::RunOptions run_options(nullptr);

    for (size_t b = 0; b < images.size(); b++) {
        if (images[b].empty()) continue;

        // Preprocess
        std::vector<float> blob;
        preprocess(images[b], blob);

        // Create input tensor
        std::array<int64_t, 4> input_shape_arr = {
            1, 3,
            static_cast<int64_t>(input_height_),
            static_cast<int64_t>(input_width_)
        };
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            mem_info_, blob.data(), blob.size(), input_shape_arr.data(), 4);

        // Run inference
        auto output_tensors = session_->Run(run_options,
            input_names_.data(), &input_tensor, 1,
            output_names_.data(), output_names_.size());

        // Get output data and shape
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();

        // Postprocess
        std::vector<float> output_vec(output_data,
            output_data + output_tensors[0].GetTensorTypeAndShapeInfo().GetElementCount());
        postprocess(output_vec, output_shape, images[b].size(), detections[b]);
    }
}

} // namespace cvedix_yolo_ort
