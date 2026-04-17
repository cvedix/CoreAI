/**
 * @file onnx_yolov26_detector.cpp
 * @brief OpenCV DNN-based YOLOv26 detector implementation
 */

#include "onnx_yolov26_detector.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace onnx_yolov26 {

namespace {

cv::Mat reshape_detection_output(cv::Mat output, int& num_classes, bool& end2end) {
    end2end = false;

    if (output.dims == 3) {
        const int dim1 = output.size[1];
        const int dim2 = output.size[2];

        if (dim2 == 6) {
            end2end = true;
            return output.reshape(1, dim1);
        }
        if (dim1 == 6) {
            end2end = true;
            return output.reshape(0, dim1).t();
        }
        if (dim1 > dim2) {
            num_classes = dim2 - 4;
            return output.reshape(1, dim1);
        }

        num_classes = dim1 - 4;
        return output.reshape(0, dim1).t();
    }

    if (output.dims == 2) {
        if (output.cols == 6) {
            end2end = true;
            return output;
        }
        if (output.rows == 6) {
            end2end = true;
            return output.t();
        }
        if (output.rows > output.cols) {
            num_classes = output.cols - 4;
            return output;
        }

        num_classes = output.rows - 4;
        return output.t();
    }

    if (output.total() > 0) {
        const int dims = 4 + num_classes;
        return output.reshape(1, static_cast<int>(output.total() / dims));
    }

    return cv::Mat();
}

} // namespace

onnx_yolov26_detector::onnx_yolov26_detector(const std::string& onnx_path,
                                             float conf_threshold,
                                             float nms_threshold)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {
    if (!load_model(onnx_path)) {
        throw std::runtime_error("Failed to load ONNX model: " + onnx_path);
    }

    std::cout << "[onnx_yolov26] Loaded ONNX model: " << onnx_path << std::endl;
    std::cout << "[onnx_yolov26] Input: " << input_width << "x" << input_height
              << ", Classes: " << num_classes << std::endl;
}

onnx_yolov26_detector::~onnx_yolov26_detector() = default;

bool onnx_yolov26_detector::load_model(const std::string& onnx_path) {
    std::ifstream file(onnx_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[onnx_yolov26] Model file not found: " << onnx_path << std::endl;
        return false;
    }
    file.close();

    try {
        net = cv::dnn::readNetFromONNX(onnx_path);
        if (net.empty()) {
            std::cerr << "[onnx_yolov26] Failed to load ONNX model" << std::endl;
            return false;
        }

        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        std::vector<std::string> output_names = net.getUnconnectedOutLayersNames();
        if (output_names.empty()) {
            std::cerr << "[onnx_yolov26] No output layers found" << std::endl;
            return false;
        }

        output_layer_name = output_names[0];
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "[onnx_yolov26] OpenCV DNN Error: " << e.what() << std::endl;
        return false;
    }
}

void onnx_yolov26_detector::preprocess(const cv::Mat& image, cv::Mat& blob) {
    const int orig_w = image.cols;
    const int orig_h = image.rows;

    const float scale = std::min(
        static_cast<float>(input_width) / orig_w,
        static_cast<float>(input_height) / orig_h);

    const int new_w = static_cast<int>(std::round(orig_w * scale));
    const int new_h = static_cast<int>(std::round(orig_h * scale));

    const int pad_w = (input_width - new_w) / 2;
    const int pad_h = (input_height - new_h) / 2;

    letterbox_scale = scale;
    letterbox_pad_x = static_cast<float>(pad_w);
    letterbox_pad_y = static_cast<float>(pad_h);

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(new_w, new_h));

    cv::Mat padded(input_height, input_width, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(pad_w, pad_h, new_w, new_h)));

    blob = cv::dnn::blobFromImage(
        padded,
        1.0 / 255.0,
        cv::Size(input_width, input_height),
        cv::Scalar(0, 0, 0),
        true,
        false,
        CV_32F);
}

void onnx_yolov26_detector::postprocess_end2end(const cv::Mat& output,
                                                const cv::Size& original_size,
                                                std::vector<Detection>& detections) {
    detections.clear();

    for (int i = 0; i < output.rows; ++i) {
        const float* row = output.ptr<float>(i);
        const float conf = row[4];
        if (conf < conf_threshold) {
            continue;
        }

        float x1 = (row[0] - letterbox_pad_x) / letterbox_scale;
        float y1 = (row[1] - letterbox_pad_y) / letterbox_scale;
        float x2 = (row[2] - letterbox_pad_x) / letterbox_scale;
        float y2 = (row[3] - letterbox_pad_y) / letterbox_scale;

        x1 = std::max(0.0f, std::min(x1, static_cast<float>(original_size.width)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(original_size.height)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(original_size.width)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(original_size.height)));

        Detection det;
        det.bbox[0] = (x1 + x2) * 0.5f;
        det.bbox[1] = (y1 + y2) * 0.5f;
        det.bbox[2] = x2 - x1;
        det.bbox[3] = y2 - y1;
        det.conf = conf;
        det.class_id = static_cast<int>(row[5]);
        detections.push_back(det);
    }
}

void onnx_yolov26_detector::postprocess_traditional(const cv::Mat& output,
                                                    const cv::Size& original_size,
                                                    std::vector<Detection>& detections) {
    detections.clear();

    for (int i = 0; i < output.rows; ++i) {
        const float* row = output.ptr<float>(i);

        float best_score = 0.0f;
        int best_class = 0;
        for (int c = 0; c < num_classes; ++c) {
            const float score = row[4 + c];
            if (score > best_score) {
                best_score = score;
                best_class = c;
            }
        }

        if (best_score < conf_threshold) {
            continue;
        }

        float x1 = row[0] - row[2] * 0.5f;
        float y1 = row[1] - row[3] * 0.5f;
        float x2 = row[0] + row[2] * 0.5f;
        float y2 = row[1] + row[3] * 0.5f;

        x1 = (x1 - letterbox_pad_x) / letterbox_scale;
        y1 = (y1 - letterbox_pad_y) / letterbox_scale;
        x2 = (x2 - letterbox_pad_x) / letterbox_scale;
        y2 = (y2 - letterbox_pad_y) / letterbox_scale;

        x1 = std::max(0.0f, std::min(x1, static_cast<float>(original_size.width)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(original_size.height)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(original_size.width)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(original_size.height)));

        Detection det;
        det.bbox[0] = (x1 + x2) * 0.5f;
        det.bbox[1] = (y1 + y2) * 0.5f;
        det.bbox[2] = x2 - x1;
        det.bbox[3] = y2 - y1;
        det.conf = best_score;
        det.class_id = best_class;
        detections.push_back(det);
    }

    apply_nms(detections);
}

void onnx_yolov26_detector::apply_nms(std::vector<Detection>& detections) {
    if (detections.empty()) {
        return;
    }

    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) { return a.conf > b.conf; });

    std::vector<bool> suppressed(detections.size(), false);
    std::vector<Detection> kept;

    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) {
            continue;
        }

        kept.push_back(detections[i]);

        const float ax1 = detections[i].bbox[0] - detections[i].bbox[2] * 0.5f;
        const float ay1 = detections[i].bbox[1] - detections[i].bbox[3] * 0.5f;
        const float ax2 = detections[i].bbox[0] + detections[i].bbox[2] * 0.5f;
        const float ay2 = detections[i].bbox[1] + detections[i].bbox[3] * 0.5f;
        const float area_a = detections[i].bbox[2] * detections[i].bbox[3];

        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j] || detections[i].class_id != detections[j].class_id) {
                continue;
            }

            const float bx1 = detections[j].bbox[0] - detections[j].bbox[2] * 0.5f;
            const float by1 = detections[j].bbox[1] - detections[j].bbox[3] * 0.5f;
            const float bx2 = detections[j].bbox[0] + detections[j].bbox[2] * 0.5f;
            const float by2 = detections[j].bbox[1] + detections[j].bbox[3] * 0.5f;
            const float area_b = detections[j].bbox[2] * detections[j].bbox[3];

            const float ix1 = std::max(ax1, bx1);
            const float iy1 = std::max(ay1, by1);
            const float ix2 = std::min(ax2, bx2);
            const float iy2 = std::min(ay2, by2);

            const float iw = std::max(0.0f, ix2 - ix1);
            const float ih = std::max(0.0f, iy2 - iy1);
            const float inter = iw * ih;
            const float iou = inter / (area_a + area_b - inter + 1e-6f);

            if (iou > nms_threshold) {
                suppressed[j] = true;
            }
        }
    }

    detections = std::move(kept);
}

void onnx_yolov26_detector::detect(const cv::Mat& image, std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = batch_detections[0];
    }
}

void onnx_yolov26_detector::detect(const std::vector<cv::Mat>& images,
                                   std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());

    for (size_t index = 0; index < images.size(); ++index) {
        if (images[index].empty()) {
            continue;
        }

        cv::Mat blob;
        preprocess(images[index], blob);

        net.setInput(blob);
        cv::Mat output = net.forward(output_layer_name);

        bool end2end = false;
        cv::Mat rows = reshape_detection_output(output, num_classes, end2end);
        if (rows.empty()) {
            continue;
        }

        num_boxes = rows.rows;
        if (end2end) {
            postprocess_end2end(rows, images[index].size(), detections[index]);
        } else {
            postprocess_traditional(rows, images[index].size(), detections[index]);
        }
    }
}

cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h) {
    (void)input_w;
    (void)input_h;

    const int x = std::max(0, static_cast<int>(bbox[0] - bbox[2] * 0.5f));
    const int y = std::max(0, static_cast<int>(bbox[1] - bbox[3] * 0.5f));
    const int width = std::min(static_cast<int>(bbox[2]), img.cols - x);
    const int height = std::min(static_cast<int>(bbox[3]), img.rows - y);

    return cv::Rect(x, y, width, height);
}

} // namespace onnx_yolov26