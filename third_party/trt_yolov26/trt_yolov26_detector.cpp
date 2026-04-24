/**
 * @file trt_yolov26_detector.cpp
 * @brief TensorRT YOLOv26 detector implementation
 */

#include "trt_yolov26_detector.h"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace trt_yolov26 {

namespace {

std::vector<int> non_batch_dims(const nvinfer1::Dims& dims) {
    std::vector<int> values;
    for (int i = 0; i < dims.nbDims; ++i) {
        if (dims.d[i] > 1) {
            values.push_back(dims.d[i]);
        }
    }
    if (!values.empty() && values.front() == 1) {
        values.erase(values.begin());
    }
    return values;
}

size_t tensor_element_count(const nvinfer1::Dims& dims) {
    size_t total = 1;
    for (int i = 0; i < dims.nbDims; ++i) {
        total *= static_cast<size_t>(std::max<int64_t>(dims.d[i], 1));
    }
    return total;
}

} // namespace

void trt_yolov26_detector::Logger::log(Severity severity, const char* msg) noexcept {
    if (severity <= Severity::kWARNING) {
        std::cerr << "[TRT] " << msg << std::endl;
    }
}

trt_yolov26_detector::trt_yolov26_detector(const std::string& engine_path,
                                           float conf_threshold,
                                           float nms_threshold)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {
    cudaStreamCreate(&stream);

    if (!load_engine(engine_path)) {
        throw std::runtime_error("Failed to load TensorRT engine: " + engine_path);
    }

    allocate_buffers();
}

trt_yolov26_detector::~trt_yolov26_detector() {
    if (stream) {
        cudaStreamDestroy(stream);
    }
    for (void* buffer : device_buffers) {
        if (buffer) {
            cudaFree(buffer);
        }
    }
    delete[] host_output;
    delete context;
    delete engine;
    delete runtime;
}

bool trt_yolov26_detector::load_engine(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[trt_yolov26] Engine file not found: " << engine_path << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    const size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<char> engine_data(size);
    file.read(engine_data.data(), static_cast<std::streamsize>(size));
    file.close();

    runtime = nvinfer1::createInferRuntime(logger);
    if (!runtime) {
        return false;
    }
    engine = runtime->deserializeCudaEngine(engine_data.data(), size);
    if (!engine) {
        return false;
    }
    context = engine->createExecutionContext();
    if (!context) {
        return false;
    }

    num_io_tensors = engine->getNbIOTensors();
    tensor_sizes.resize(num_io_tensors, 0);

    for (int i = 0; i < num_io_tensors; ++i) {
        const char* name = engine->getIOTensorName(i);
        const auto dims = engine->getTensorShape(name);
        tensor_sizes[i] = tensor_element_count(dims);

        if (engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) {
            input_tensor_idx = i;
            const std::vector<int> dims_no_batch = non_batch_dims(dims);
            if (dims_no_batch.size() >= 3) {
                input_height = dims_no_batch[dims_no_batch.size() - 2];
                input_width = dims_no_batch[dims_no_batch.size() - 1];
            }
            continue;
        }

        if (output_tensor_idx == -1) {
            output_tensor_idx = i;
            const std::vector<int> dims_no_batch = non_batch_dims(dims);
            if (dims_no_batch.size() >= 2) {
                const int dim1 = dims_no_batch[dims_no_batch.size() - 2];
                const int dim2 = dims_no_batch[dims_no_batch.size() - 1];

                if (dim2 == 6) {
                    end2end_output = true;
                    output_rows = dim1;
                    output_cols = dim2;
                    transpose_output = false;
                } else if (dim1 == 6) {
                    end2end_output = true;
                    output_rows = dim2;
                    output_cols = dim1;
                    transpose_output = true;
                } else if (dim1 > dim2) {
                    output_rows = dim1;
                    output_cols = dim2;
                    transpose_output = false;
                    num_classes = output_cols - 4;
                } else {
                    output_rows = dim2;
                    output_cols = dim1;
                    transpose_output = true;
                    num_classes = output_cols - 4;
                }
                num_boxes = output_rows;
            }
        }
    }

    return input_tensor_idx >= 0 && output_tensor_idx >= 0 && output_rows > 0 && output_cols > 0;
}

void trt_yolov26_detector::allocate_buffers() {
    device_buffers.resize(num_io_tensors, nullptr);

    for (int i = 0; i < num_io_tensors; ++i) {
        cudaMalloc(&device_buffers[i], tensor_sizes[i] * sizeof(float));
    }

    host_output = new float[output_rows * output_cols];
}

void trt_yolov26_detector::preprocess(const cv::Mat& image, float* input_buffer) {
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

float trt_yolov26_detector::output_at(int row, int col) const {
    if (transpose_output) {
        return host_output[col * output_rows + row];
    }
    return host_output[row * output_cols + col];
}

void trt_yolov26_detector::postprocess(const cv::Size& original_size,
                                       std::vector<Detection>& detections) {
    detections.clear();

    for (int row = 0; row < output_rows; ++row) {
        Detection det{};

        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;

        if (end2end_output) {
            det.conf = output_at(row, 4);
            if (det.conf < conf_threshold) {
                continue;
            }

            det.class_id = static_cast<int>(output_at(row, 5));
            x1 = output_at(row, 0);
            y1 = output_at(row, 1);
            x2 = output_at(row, 2);
            y2 = output_at(row, 3);
        } else {
            float best_score = 0.0f;
            int best_class = 0;
            for (int cls = 0; cls < num_classes; ++cls) {
                const float score = output_at(row, 4 + cls);
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

            const float cx = output_at(row, 0);
            const float cy = output_at(row, 1);
            const float w = output_at(row, 2);
            const float h = output_at(row, 3);
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
        detections.push_back(det);
    }

    if (!end2end_output) {
        apply_nms(detections);
    }
}

void trt_yolov26_detector::apply_nms(std::vector<Detection>& detections) {
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

void trt_yolov26_detector::detect(const cv::Mat& image, std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = batch_detections[0];
    }
}

void trt_yolov26_detector::detect(const std::vector<cv::Mat>& images,
                                  std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());

    for (size_t index = 0; index < images.size(); ++index) {
        if (images[index].empty()) {
            continue;
        }

        std::vector<float> input_data(3 * input_height * input_width);
        preprocess(images[index], input_data.data());

        cudaMemcpyAsync(device_buffers[input_tensor_idx], input_data.data(),
                        input_data.size() * sizeof(float), cudaMemcpyHostToDevice, stream);

        for (int i = 0; i < num_io_tensors; ++i) {
            context->setTensorAddress(engine->getIOTensorName(i), device_buffers[i]);
        }

        context->enqueueV3(stream);

        cudaMemcpyAsync(host_output, device_buffers[output_tensor_idx],
                        static_cast<size_t>(output_rows * output_cols) * sizeof(float),
                        cudaMemcpyDeviceToHost, stream);
        cudaStreamSynchronize(stream);

        postprocess(images[index].size(), detections[index]);
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

} // namespace trt_yolov26