/**
 * @file ov_yolov26_detector.cpp
 * @brief OpenVINO YOLOv26 detector implementation
 */

#ifdef CVEDIX_WITH_OPENVINO

#include "ov_yolov26_detector.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

namespace ov_yolov26 {

namespace {

std::vector<size_t> non_batch_dims(const ov::Shape& shape) {
    std::vector<size_t> dims;
    for (size_t value : shape) {
        if (value > 1) {
            dims.push_back(value);
        }
    }
    if (!dims.empty() && dims.front() == 1) {
        dims.erase(dims.begin());
    }
    return dims;
}

} // namespace

ov_yolov26_detector::ov_yolov26_detector(const std::string& model_path,
                                         const std::string& device,
                                         float conf_threshold,
                                         float nms_threshold,
                                         const std::string& labels_path)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {
    if (!load_model(model_path, device)) {
        throw std::runtime_error("Failed to load OpenVINO model: " + model_path);
    }

    std::string metadata_path = "metadata.yaml";
    const size_t last_slash = model_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        metadata_path = model_path.substr(0, last_slash + 1) + "metadata.yaml";
    }
    load_metadata_yaml(metadata_path);
    if (!labels_path.empty()) {
        load_labels(labels_path);
    }
}

ov_yolov26_detector::~ov_yolov26_detector() = default;

bool ov_yolov26_detector::load_model(const std::string& model_path, const std::string& device) {
    try {
        model = core.read_model(model_path);
        if (!model) {
            return false;
        }

        const auto input_shape = model->input().get_shape();
        const std::vector<size_t> input_dims = non_batch_dims(input_shape);
        if (input_dims.size() >= 3) {
            input_height = static_cast<int>(input_dims[input_dims.size() - 2]);
            input_width = static_cast<int>(input_dims[input_dims.size() - 1]);
        }

        const auto output_shape = model->output().get_shape();
        const std::vector<size_t> output_dims = non_batch_dims(output_shape);
        if (output_dims.size() >= 2) {
            const int dim1 = static_cast<int>(output_dims[output_dims.size() - 2]);
            const int dim2 = static_cast<int>(output_dims[output_dims.size() - 1]);

            if (dim2 == 6) {
                end2end_output = true;
                output_rows = dim1;
                output_cols = dim2;
            } else if (dim1 == 6) {
                end2end_output = true;
                output_rows = dim2;
                output_cols = dim1;
                transpose_output = true;
            } else if (dim1 > dim2) {
                output_rows = dim1;
                output_cols = dim2;
                num_classes = output_cols - 4;
            } else {
                output_rows = dim2;
                output_cols = dim1;
                transpose_output = true;
                num_classes = output_cols - 4;
            }
            num_boxes = output_rows;
        }

        ov::preprocess::PrePostProcessor ppp(model);
        ppp.input().tensor().set_element_type(ov::element::u8).set_layout("NHWC");
        ppp.input().tensor().set_color_format(ov::preprocess::ColorFormat::BGR);
        ppp.input().preprocess()
            .convert_color(ov::preprocess::ColorFormat::RGB)
            .convert_element_type(ov::element::f32)
            .scale(255.0f);
        ppp.input().model().set_layout("NCHW");
        ppp.output().tensor().set_element_type(ov::element::f32);
        model = ppp.build();

        compiled_model = core.compile_model(model, device);
        infer_request = compiled_model.create_infer_request();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ov_yolov26] Exception loading model: " << e.what() << std::endl;
        return false;
    }
}

void ov_yolov26_detector::preprocess(const cv::Mat& image, float* input_buffer) {
    const float scale = std::min(static_cast<float>(input_width) / image.cols,
                                 static_cast<float>(input_height) / image.rows);
    const int scaled_w = static_cast<int>(image.cols * scale);
    const int scaled_h = static_cast<int>(image.rows * scale);
    const int pad_left = (input_width - scaled_w) / 2;
    const int pad_top = (input_height - scaled_h) / 2;

    letterbox_scale = scale;
    letterbox_pad_x = static_cast<float>(pad_left);
    letterbox_pad_y = static_cast<float>(pad_top);

    cv::Mat canvas(input_height, input_width, CV_8UC3, cv::Scalar(114, 114, 114));
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(scaled_w, scaled_h));
    resized.copyTo(canvas(cv::Rect(pad_left, pad_top, scaled_w, scaled_h)));

    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);

    cv::Mat float_img;
    rgb.convertTo(float_img, CV_32FC3, 1.0 / 255.0);

    std::vector<cv::Mat> channels(3);
    cv::split(float_img, channels);
    const int channel_size = input_height * input_width;
    for (int c = 0; c < 3; ++c) {
        std::memcpy(input_buffer + c * channel_size, channels[c].data, channel_size * sizeof(float));
    }
}

float ov_yolov26_detector::output_at(const float* output, int row, int col) const {
    if (transpose_output) {
        return output[col * output_rows + row];
    }
    return output[row * output_cols + col];
}

void ov_yolov26_detector::postprocess(const float* output,
                                      std::vector<Detection>& detections,
                                      const cv::Size& original_size) {
    detections.clear();

    for (int row = 0; row < output_rows; ++row) {
        Detection det{};

        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;

        if (end2end_output) {
            det.conf = output_at(output, row, 4);
            if (det.conf < conf_threshold) {
                continue;
            }
            det.class_id = static_cast<int>(output_at(output, row, 5));
            x1 = output_at(output, row, 0);
            y1 = output_at(output, row, 1);
            x2 = output_at(output, row, 2);
            y2 = output_at(output, row, 3);
        } else {
            float best_score = 0.0f;
            int best_class = 0;
            for (int cls = 0; cls < num_classes; ++cls) {
                const float score = output_at(output, row, 4 + cls);
                if (score > best_score) {
                    best_score = score;
                    best_class = cls;
                }
            }
            if (best_score < conf_threshold) {
                continue;
            }

            det.conf = best_score;
            det.class_id = best_class;

            const float cx = output_at(output, row, 0);
            const float cy = output_at(output, row, 1);
            const float w = output_at(output, row, 2);
            const float h = output_at(output, row, 3);
            x1 = cx - w * 0.5f;
            y1 = cy - h * 0.5f;
            x2 = cx + w * 0.5f;
            y2 = cy + h * 0.5f;
        }

        x1 = (x1 - letterbox_pad_x) / letterbox_scale;
        y1 = (y1 - letterbox_pad_y) / letterbox_scale;
        x2 = (x2 - letterbox_pad_x) / letterbox_scale;
        y2 = (y2 - letterbox_pad_y) / letterbox_scale;

        x1 = std::max(0.0f, std::min(x1, static_cast<float>(original_size.width)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(original_size.height)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(original_size.width)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(original_size.height)));

        det.bbox[0] = (x1 + x2) * 0.5f;
        det.bbox[1] = (y1 + y2) * 0.5f;
        det.bbox[2] = x2 - x1;
        det.bbox[3] = y2 - y1;

        if (det.class_id >= 0 && det.class_id < static_cast<int>(labels.size()) && !labels[det.class_id].empty()) {
            det.class_name = labels[det.class_id];
        }

        detections.push_back(det);
    }

    if (!end2end_output) {
        apply_nms(detections);
    }
}

void ov_yolov26_detector::apply_nms(std::vector<Detection>& detections) {
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

void ov_yolov26_detector::detect(const cv::Mat& image, std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = batch_detections[0];
    }
}

void ov_yolov26_detector::detect(const std::vector<cv::Mat>& images,
                                 std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());

    for (size_t index = 0; index < images.size(); ++index) {
        if (images[index].empty()) {
            continue;
        }

        std::vector<float> input_data(3 * input_height * input_width);
        preprocess(images[index], input_data.data());

        cv::Mat letterbox(input_height, input_width, CV_8UC3);
        for (int c = 0; c < 3; ++c) {
            for (int h = 0; h < input_height; ++h) {
                for (int w = 0; w < input_width; ++w) {
                    const int idx = h * input_width + w;
                    letterbox.at<cv::Vec3b>(h, w)[c] = static_cast<uint8_t>(
                        input_data[c * input_height * input_width + idx] * 255.0f);
                }
            }
        }

        ov::Tensor input_tensor(ov::element::u8,
                                {1, static_cast<size_t>(input_height), static_cast<size_t>(input_width), 3},
                                letterbox.data);
        infer_request.set_input_tensor(input_tensor);
        infer_request.infer();

        const auto output_tensor = infer_request.get_output_tensor();
        postprocess(output_tensor.data<float>(), detections[index], images[index].size());
    }
}

bool ov_yolov26_detector::load_labels(const std::string& labels_path) {
    labels.clear();

    std::ifstream file(labels_path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            labels.push_back(line);
        }
    }
    return !labels.empty();
}

bool ov_yolov26_detector::load_metadata_yaml(const std::string& yaml_path) {
    std::ifstream file(yaml_path);
    if (!file.is_open()) {
        return false;
    }

    labels.clear();
    std::map<int, std::string> class_map;
    std::string line;
    bool in_names = false;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line == "names:") {
            in_names = true;
            continue;
        }
        if (!in_names || line.find(':') == std::string::npos) {
            continue;
        }

        const size_t pos = line.find(':');
        try {
            class_map[std::stoi(line.substr(0, pos))] = line.substr(pos + 1);
        } catch (...) {
        }
    }

    if (class_map.empty()) {
        return false;
    }

    labels.resize(class_map.rbegin()->first + 1);
    for (const auto& pair : class_map) {
        std::string value = pair.second;
        value.erase(0, value.find_first_not_of(" \t"));
        labels[pair.first] = value;
    }
    return true;
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

} // namespace ov_yolov26

#endif // CVEDIX_WITH_OPENVINO