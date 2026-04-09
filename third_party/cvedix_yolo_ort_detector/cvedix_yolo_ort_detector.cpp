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
    // Common YOLO output shapes:
    // [1, 84, 8400]  → YOLOv11 transposed single-head
    // [1, 8400, 84]  → YOLOv11 single-head
    // [1, 84, N]     → YOLOv11 where N = 3*(80*80 + 40*40 + 20*20) = 25200 etc.
    // [1, N, 85]     → YOLOv8/YOLOv5 single-head (4 bbox + 1 conf + 80 classes)
    // [1, 85, N]     → YOLOv8/YOLOv5 transposed

    // Query output shape from session
    Ort::TypeInfo output_info = session_->GetOutputTypeInfo(0);
    auto output_shape_info = output_info.GetTensorTypeAndShapeInfo();
    auto output_shape = output_shape_info.GetShape();

    int batch = 1;
    int dim0 = 0, dim1 = 0, dim2 = 0;

    if (output_shape.size() == 3) {
        batch = static_cast<int>(output_shape[0]);
        dim0 = static_cast<int>(output_shape[1]);
        dim1 = static_cast<int>(output_shape[2]);
    } else if (output_shape.size() == 2) {
        batch = static_cast<int>(output_shape[0]);
        dim0 = static_cast<int>(output_shape[1]);
    }

    // Detect format
    // [batch, 84, N] or [batch, 85, N] → transposed
    // [batch, N, 84] or [batch, N, 85] → not transposed
    // [batch, num_classes + 4, N] → transposed
    // [batch, N, num_classes + 4] → not transposed

    bool transposed = false;

    // Common class + bbox dims
    int bbox_plus_conf = 4 + 1;  // 4 coords + 1 conf

    // Check if dim0 is the class+bbox dim
    int possible_classes_dim0 = dim0 - bbox_plus_conf;  // if it's 80, dim0=85
    int possible_classes_dim1 = dim1 - bbox_plus_conf;  // if it's 80, dim1=85

    if (possible_classes_dim0 > 0 && possible_classes_dim0 <= 128) {
        // dim0 is [num_classes+bbox], likely transposed: [B, 85, N]
        transposed = true;
        num_classes_ = possible_classes_dim0;
        num_anchors_ = dim1;
        output_transposed_ = true;
    } else if (possible_classes_dim1 > 0 && possible_classes_dim1 <= 128) {
        // dim1 is [num_classes+bbox], standard format: [B, N, 85]
        transposed = false;
        num_classes_ = possible_classes_dim1;
        num_anchors_ = dim0;
        output_transposed_ = false;
    } else if (dim0 == 84 || dim0 == 85) {
        // dim0 == 84 → YOLOv11, transposed [B, 84, N]
        // dim0 == 85 → YOLOv5/v8 1-class, transposed [B, 85, N]
        transposed = true;
        if (dim0 == 85) num_classes_ = 1;
        num_anchors_ = dim1;
        output_transposed_ = true;
    } else if (dim1 == 84 || dim1 == 85) {
        // dim1 == 84 → YOLOv11, standard [B, N, 84]
        // dim1 == 85 → YOLOv5/v8 1-class, standard [B, N, 85]
        transposed = false;
        if (dim1 == 85) num_classes_ = 1;
        num_anchors_ = dim0;
        output_transposed_ = false;
    } else {
        // Fallback: assume YOLOv11 format [1, 84, N]
        transposed = true;
        num_anchors_ = dim1;
        output_transposed_ = true;
    }

    if (num_classes_ <= 0) num_classes_ = 80;
    if (num_anchors_ <= 0) num_anchors_ = 8400;
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
    int stride = 4 + 1 + num_classes_;  // 4 bbox + 1 conf + num_classes

    // Access data
    float* data = output_data.data();

    for (int a = 0; a < num_anchors; a++) {
        float* anchor_data;

        if (output_transposed_) {
            // Shape: [1, stride, num_anchors] — data indexed as [anchor * stride + dim]
            anchor_data = &data[a * stride];
        } else {
            // Shape: [1, num_anchors, stride] — data indexed as [anchor * stride + dim]
            anchor_data = &data[a * stride];
        }

        // Get box confidence
        float box_conf = sigmoid(anchor_data[4]);

        // Find best class
        int best_class = 0;
        float max_class_conf = 0.0f;

        for (int c = 0; c < num_classes_; c++) {
            float cls_conf = sigmoid(anchor_data[5 + c]);
            float score = box_conf * cls_conf;
            if (score > max_class_conf) {
                max_class_conf = score;
                best_class = c;
            }
        }

        if (max_class_conf < conf_threshold_) continue;

        // Decode bbox (center/xywh in output space)
        float cx = anchor_data[0];
        float cy = anchor_data[1];
        float w = anchor_data[2];
        float h = anchor_data[3];

        // Apply sigmoid to bbox coords (YOLOv11 uses raw outputs)
        cx = sigmoid(cx);
        cy = sigmoid(cy);
        // w, h can be sigmoid or exp depending on model

        // Convert to letterbox image space (absolute pixels)
        float img_cx = cx * input_width_;
        float img_cy = cy * input_height_;
        float img_w = w * input_width_;
        float img_h = h * input_height_;

        // Remove letterbox padding and scale to original image
        float orig_cx = (img_cx - letterbox_pad_x_) / letterbox_scale_;
        float orig_cy = (img_cy - letterbox_pad_y_) / letterbox_scale_;
        float orig_w = img_w / letterbox_scale_;
        float orig_h = img_h / letterbox_scale_;

        // Clamp to bounds
        orig_cx = std::max(0.0f, std::min(orig_cx, static_cast<float>(original_size.width)));
        orig_cy = std::max(0.0f, std::min(orig_cy, static_cast<float>(original_size.height)));
        orig_w = std::max(0.0f, std::min(orig_w, static_cast<float>(original_size.width)));
        orig_h = std::max(0.0f, std::min(orig_h, static_cast<float>(original_size.height)));

        Detection det;
        det.bbox[0] = orig_cx;
        det.bbox[1] = orig_cy;
        det.bbox[2] = orig_w;
        det.bbox[3] = orig_h;
        det.conf = max_class_conf;
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
