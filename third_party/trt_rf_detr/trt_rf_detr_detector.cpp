/**
 * @file trt_rf_detr_detector.cpp
 * @brief TensorRT RF-DETR detector – multi-head DFL + fused fallback
 */

#include "trt_rf_detr_detector.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <chrono>

namespace trt_rf_detr {

// ──────────────────────── Logger ────────────────────────
void trt_rf_detr_detector::Logger::log(Severity severity, const char* msg) noexcept {
    if (severity <= Severity::kWARNING) {
        std::cerr << "[TRT] " << msg << std::endl;
    }
}

// ──────────────────────── DFL decode ────────────────────
// raw: pointer to 64 floats (4 groups × 16 bins)
// out: 4 decoded offsets (left, top, right, bottom)
void trt_rf_detr_detector::dfl_decode(const float* raw, float out[4]) {
    for (int i = 0; i < 4; ++i) {
        const float* bins = raw + i * DFL_BINS;

        // softmax
        float max_val = *std::max_element(bins, bins + DFL_BINS);
        float sum_exp = 0.0f;
        float vals[DFL_BINS];
        for (int j = 0; j < DFL_BINS; ++j) {
            vals[j] = std::exp(bins[j] - max_val);
            sum_exp += vals[j];
        }

        // weighted sum  E[x] = sum(softmax(x) * [0..15])
        float result = 0.0f;
        for (int j = 0; j < DFL_BINS; ++j) {
            result += (vals[j] / sum_exp) * static_cast<float>(j);
        }
        out[i] = result;
    }
}

// ──────────────────────── Constructor ──────────────────
trt_rf_detr_detector::trt_rf_detr_detector(const std::string& engine_path,
                                           float conf_threshold,
                                           float nms_threshold)
    : conf_threshold(conf_threshold), nms_threshold(nms_threshold) {

    cudaStreamCreate(&stream);

    if (!load_engine(engine_path)) {
        throw std::runtime_error("Failed to load TensorRT engine: " + engine_path);
    }

    allocate_buffers();

    std::cout << "[trt_rf_detr] Loaded engine: " << engine_path << std::endl;
    std::cout << "[trt_rf_detr] Input: " << input_width << "x" << input_height
              << ", Classes: " << num_classes << ", Boxes: " << num_boxes
              << ", Mode: " << (multi_head ? "multi-head" : "fused") << std::endl;
}

// ──────────────────────── Destructor ──────────────────
trt_rf_detr_detector::~trt_rf_detr_detector() {
    cudaStreamDestroy(stream);

    for (auto buf : device_buffers) {
        if (buf) cudaFree(buf);
    }
    for (auto h : host_outputs) {
        delete[] h;
    }
    if (host_output_fused) delete[] host_output_fused;
    if (host_input_buffer) cudaFreeHost(host_input_buffer);

    if (context) delete context;
    if (engine)  delete engine;
    if (runtime) delete runtime;
}

// ──────────────────────── load_engine ──────────────────
bool trt_rf_detr_detector::load_engine(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[trt_rf_detr] Engine file not found: " << engine_path << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> engine_data(size);
    file.read(engine_data.data(), size);
    file.close();

    runtime = nvinfer1::createInferRuntime(logger);
    if (!runtime) return false;

    engine = runtime->deserializeCudaEngine(engine_data.data(), size);
    if (!engine) return false;

    context = engine->createExecutionContext();
    if (!context) return false;

    // ---- Enumerate IO tensors ----
    num_io_tensors = engine->getNbIOTensors();

    for (int i = 0; i < num_io_tensors; ++i) {
        auto name_cstr = engine->getIOTensorName(i);
        std::string name(name_cstr);
        auto mode = engine->getTensorIOMode(name_cstr);
        if (mode == nvinfer1::TensorIOMode::kOUTPUT) {
            if (name == "pred_boxes" || name == "boxes") {
                detr_boxes_idx = i;
                auto dims = engine->getTensorShape(name_cstr);
                detr_num_queries = dims.d[1];
            } else if (name == "logits" || name == "scores" || name == "labels") {
                detr_logits_idx = i;
                auto dims = engine->getTensorShape(name_cstr);
                num_classes = dims.d[2];
            }
        }
    }

    if (detr_boxes_idx >= 0 && detr_logits_idx >= 0) {
        is_detr = true;
        multi_head = false;
        num_boxes = detr_num_queries;
        std::cout << "[trt_rf_detr] Mode: DETR (queries=" << detr_num_queries << ", classes=" << num_classes << ")" << std::endl;
    } else if (num_io_tensors > 2) {
        multi_head = true;
    }

    // Find input tensor
    for (int i = 0; i < num_io_tensors; ++i) {
        auto name = engine->getIOTensorName(i);
        auto mode = engine->getTensorIOMode(name);
        if (mode == nvinfer1::TensorIOMode::kINPUT) {
            input_tensor_idx = i;
            auto dims = engine->getTensorShape(name);
            input_height = dims.d[2];
            input_width  = dims.d[3];
            break;
        }
    }

    if (multi_head) {
        // ---- Multi-head: find reg* and cls* tensors ----
        // We expect pairs: (reg1,cls1), (reg2,cls2), (reg3,cls3)
        // reg has 64 channels (DFL), cls has num_classes channels

        struct TensorEntry {
            int idx;
            std::string name;
            nvinfer1::Dims dims;
        };

        std::vector<TensorEntry> reg_tensors, cls_tensors;

        for (int i = 0; i < num_io_tensors; ++i) {
            auto name_cstr = engine->getIOTensorName(i);
            std::string name(name_cstr);
            auto mode = engine->getTensorIOMode(name_cstr);
            if (mode == nvinfer1::TensorIOMode::kINPUT) continue;

            auto dims = engine->getTensorShape(name_cstr);

            // Identify by name prefix or channel count
            if (name.find("reg") != std::string::npos || dims.d[1] == 64) {
                reg_tensors.push_back({i, name, dims});
            } else if (name.find("cls") != std::string::npos || dims.d[1] != 64) {
                cls_tensors.push_back({i, name, dims});
            }
        }

        // Sort by grid size descending (largest grid = smallest stride)
        auto sort_by_grid = [](const TensorEntry& a, const TensorEntry& b) {
            return (a.dims.d[2] * a.dims.d[3]) > (b.dims.d[2] * b.dims.d[3]);
        };
        std::sort(reg_tensors.begin(), reg_tensors.end(), sort_by_grid);
        std::sort(cls_tensors.begin(), cls_tensors.end(), sort_by_grid);

        if (reg_tensors.size() < NUM_SCALES || cls_tensors.size() < NUM_SCALES) {
            std::cerr << "[trt_rf_detr] Expected " << NUM_SCALES
                      << " reg and cls tensors, got " << reg_tensors.size()
                      << " reg, " << cls_tensors.size() << " cls" << std::endl;
            return false;
        }

        num_classes = cls_tensors[0].dims.d[1];
        num_boxes = 0;

        for (int s = 0; s < NUM_SCALES; ++s) {
            scales[s].stride = STRIDES[s];
            scales[s].grid_h = reg_tensors[s].dims.d[2];
            scales[s].grid_w = reg_tensors[s].dims.d[3];
            scales[s].reg_tensor_idx = reg_tensors[s].idx;
            scales[s].cls_tensor_idx = cls_tensors[s].idx;
            scales[s].reg_size = 1;
            scales[s].cls_size = 1;
            for (int d = 0; d < reg_tensors[s].dims.nbDims; ++d)
                scales[s].reg_size *= reg_tensors[s].dims.d[d];
            for (int d = 0; d < cls_tensors[s].dims.nbDims; ++d)
                scales[s].cls_size *= cls_tensors[s].dims.d[d];

            num_boxes += scales[s].grid_h * scales[s].grid_w;

            std::cout << "[trt_rf_detr] Scale " << s
                      << ": stride=" << scales[s].stride
                      << " grid=" << scales[s].grid_h << "x" << scales[s].grid_w
                      << " reg=" << reg_tensors[s].name
                      << " cls=" << cls_tensors[s].name << std::endl;
        }
    } else if (!is_detr) {
        // ---- Fused single-head mode ----
        for (int i = 0; i < num_io_tensors; ++i) {
            auto name = engine->getIOTensorName(i);
            auto mode = engine->getTensorIOMode(name);
            if (mode == nvinfer1::TensorIOMode::kOUTPUT) {
                auto dims = engine->getTensorShape(name);
                int dimensions = dims.d[1];   // 4 + num_classes
                fused_num_boxes = dims.d[2];
                num_classes = dimensions - 4;
                num_boxes = fused_num_boxes;
                fused_output_size = dimensions * fused_num_boxes;
                break;
            }
        }
    }

    return true;
}

// ──────────────────────── allocate_buffers ──────────────
void trt_rf_detr_detector::allocate_buffers() {
    device_buffers.resize(num_io_tensors, nullptr);
    host_outputs.resize(num_io_tensors, nullptr);
    output_sizes.resize(num_io_tensors, 0);

    // Input buffer
    size_t input_bytes = 1 * 3 * input_height * input_width * sizeof(float);
    cudaMalloc(&device_buffers[input_tensor_idx], input_bytes);
    cudaMallocHost((void**)&host_input_buffer, input_bytes);

    if (is_detr) {
        output_sizes[detr_boxes_idx] = detr_num_queries * 4;
        cudaMalloc(&device_buffers[detr_boxes_idx], output_sizes[detr_boxes_idx] * sizeof(float));
        host_outputs[detr_boxes_idx] = new float[output_sizes[detr_boxes_idx]];

        output_sizes[detr_logits_idx] = detr_num_queries * num_classes;
        cudaMalloc(&device_buffers[detr_logits_idx], output_sizes[detr_logits_idx] * sizeof(float));
        host_outputs[detr_logits_idx] = new float[output_sizes[detr_logits_idx]];
    } else if (multi_head) {
        // Allocate for each output tensor
        for (int s = 0; s < NUM_SCALES; ++s) {
            int ri = scales[s].reg_tensor_idx;
            int ci = scales[s].cls_tensor_idx;

            output_sizes[ri] = scales[s].reg_size;
            output_sizes[ci] = scales[s].cls_size;

            cudaMalloc(&device_buffers[ri], scales[s].reg_size * sizeof(float));
            cudaMalloc(&device_buffers[ci], scales[s].cls_size * sizeof(float));

            host_outputs[ri] = new float[scales[s].reg_size];
            host_outputs[ci] = new float[scales[s].cls_size];
        }
    } else {
        // Fused: one output buffer
        for (int i = 0; i < num_io_tensors; ++i) {
            if (i == input_tensor_idx) continue;
            cudaMalloc(&device_buffers[i], fused_output_size * sizeof(float));
            host_output_fused = new float[fused_output_size];
            output_sizes[i] = fused_output_size;
            break;
        }
    }
}

// ──────────────────────── preprocess ──────────────────
void trt_rf_detr_detector::preprocess(const cv::Mat& image, float* input_buffer) {
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
    
    int channel_size = input_height * input_width;
    float* r_plane = input_buffer;
    float* g_plane = input_buffer + channel_size;
    float* b_plane = input_buffer + 2 * channel_size;

    const float scale_factor = 1.0f / 255.0f;

    if (rgb.isContinuous()) {
        const uint8_t* rgb_data = rgb.ptr<uint8_t>(0);
        for (int i = 0; i < channel_size; ++i) {
            r_plane[i] = rgb_data[3 * i + 0] * scale_factor;
            g_plane[i] = rgb_data[3 * i + 1] * scale_factor;
            b_plane[i] = rgb_data[3 * i + 2] * scale_factor;
        }
    } else {
        for (int r = 0; r < input_height; ++r) {
            const uint8_t* row_data = rgb.ptr<uint8_t>(r);
            for (int c = 0; c < input_width; ++c) {
                int i = r * input_width + c;
                r_plane[i] = row_data[3 * c + 0] * scale_factor;
                g_plane[i] = row_data[3 * c + 1] * scale_factor;
                b_plane[i] = row_data[3 * c + 2] * scale_factor;
            }
        }
    }
}

// ──────────────────── postprocess_multihead ──────────
void trt_rf_detr_detector::postprocess_multihead(
    const cv::Size& original_size,
    std::vector<Detection>& detections) {

    detections.clear();

    for (int s = 0; s < NUM_SCALES; ++s) {
        int gh = scales[s].grid_h;
        int gw = scales[s].grid_w;
        int stride = scales[s].stride;

        const float* reg_data = host_outputs[scales[s].reg_tensor_idx];
        const float* cls_data = host_outputs[scales[s].cls_tensor_idx];

        // reg_data layout: [1, 64, gh, gw]  → channel-first
        // cls_data layout: [1, num_classes, gh, gw]

        int spatial = gh * gw;

        for (int gy = 0; gy < gh; ++gy) {
            for (int gx = 0; gx < gw; ++gx) {
                int pos = gy * gw + gx;

                // ── Find best class score (apply sigmoid) ──
                float max_score = -1e9f;
                int best_class = 0;
                for (int c = 0; c < num_classes; ++c) {
                    float score = cls_data[c * spatial + pos];
                    if (score > max_score) {
                        max_score = score;
                        best_class = c;
                    }
                }
                max_score = sigmoid(max_score);

                if (max_score < conf_threshold) continue;

                // ── Gather 64 regression values for this anchor ──
                float reg_raw[64];
                for (int ch = 0; ch < 64; ++ch) {
                    reg_raw[ch] = reg_data[ch * spatial + pos];
                }

                // ── DFL decode → 4 offsets (left, top, right, bottom) ──
                float offsets[4];
                dfl_decode(reg_raw, offsets);

                // ── Convert to bbox (in letterbox input image coordinates) ──
                // Anchor center
                float anchor_cx = (static_cast<float>(gx) + 0.5f) * stride;
                float anchor_cy = (static_cast<float>(gy) + 0.5f) * stride;

                // ltrb offsets → xyxy
                float x1 = (anchor_cx - offsets[0] * stride);
                float y1 = (anchor_cy - offsets[1] * stride);
                float x2 = (anchor_cx + offsets[2] * stride);
                float y2 = (anchor_cy + offsets[3] * stride);

                // ── Remove letterbox padding and scale back to original image ──
                x1 = (x1 - letterbox_pad_x) / letterbox_scale;
                y1 = (y1 - letterbox_pad_y) / letterbox_scale;
                x2 = (x2 - letterbox_pad_x) / letterbox_scale;
                y2 = (y2 - letterbox_pad_y) / letterbox_scale;

                // center, width, height (in original image coords)
                float cx = (x1 + x2) * 0.5f;
                float cy = (y1 + y2) * 0.5f;
                float w  = (x2 - x1);
                float h  = (y2 - y1);

                Detection det;
                det.bbox[0] = cx;
                det.bbox[1] = cy;
                det.bbox[2] = w;
                det.bbox[3] = h;
                det.conf = max_score;
                det.class_id = best_class;

                detections.push_back(det);
            }
        }
    }

    apply_nms(detections);
}

// ──────────────────── postprocess_fused (legacy) ──────
void trt_rf_detr_detector::postprocess_fused(
    float* output, std::vector<Detection>& detections,
    const cv::Size& original_size) {

    detections.clear();

    for (int i = 0; i < fused_num_boxes; i++) {
        float max_score = 0.0f;
        int best_class = 0;

        for (int c = 0; c < num_classes; c++) {
            float score = output[(4 + c) * fused_num_boxes + i];
            if (score > max_score) {
                max_score = score;
                best_class = c;
            }
        }

        if (max_score < conf_threshold) continue;

        // Get bbox in letterbox input image coordinates
        float cx = output[0 * fused_num_boxes + i];
        float cy = output[1 * fused_num_boxes + i];
        float w  = output[2 * fused_num_boxes + i];
        float h  = output[3 * fused_num_boxes + i];

        // Remove letterbox padding and scale back to original image
        cx = (cx - letterbox_pad_x) / letterbox_scale;
        cy = (cy - letterbox_pad_y) / letterbox_scale;
        w  = w / letterbox_scale;
        h  = h / letterbox_scale;

        Detection det;
        det.bbox[0] = cx;
        det.bbox[1] = cy;
        det.bbox[2] = w;
        det.bbox[3] = h;
        det.conf = max_score;
        det.class_id = best_class;

        detections.push_back(det);
    }

    apply_nms(detections);
}

// ──────────────────────── apply_nms ──────────────────
void trt_rf_detr_detector::apply_nms(std::vector<Detection>& detections) {
    if (detections.empty()) return;

    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) { return a.conf > b.conf; });

    std::vector<bool> suppressed(detections.size(), false);
    std::vector<Detection> result;

    for (size_t i = 0; i < detections.size(); i++) {
        if (suppressed[i]) continue;
        result.push_back(detections[i]);

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

            float x1_i = std::max(x1_a, x1_b);
            float y1_i = std::max(y1_a, y1_b);
            float x2_i = std::min(x2_a, x2_b);
            float y2_i = std::min(y2_a, y2_b);

            float inter_w = std::max(0.0f, x2_i - x1_i);
            float inter_h = std::max(0.0f, y2_i - y1_i);
            float inter_area = inter_w * inter_h;

            float iou = inter_area / (area_a + area_b - inter_area + 1e-6f);

            if (iou > nms_threshold) {
                suppressed[j] = true;
            }
        }
    }

    detections = std::move(result);
}

// ──────────────────────── detect (single) ──────────────
void trt_rf_detr_detector::detect(const cv::Mat& image, std::vector<Detection>& detections) {
    std::vector<cv::Mat> images = {image};
    std::vector<std::vector<Detection>> batch_detections;
    detect(images, batch_detections);
    if (!batch_detections.empty()) {
        detections = std::move(batch_detections[0]);
    }
}

// ──────────────────────── detect (batch) ──────────────
void trt_rf_detr_detector::detect(const std::vector<cv::Mat>& images,
                                   std::vector<std::vector<Detection>>& detections) {
    detections.clear();
    detections.resize(images.size());

    if (images.empty()) return;

    for (size_t b = 0; b < images.size(); b++) {
        const cv::Mat& image = images[b];
        cv::Size original_size = image.size();

        // Preprocess
        preprocess(image, host_input_buffer);

        // Copy input to device
        size_t input_bytes = 1 * 3 * input_height * input_width * sizeof(float);
        cudaMemcpyAsync(device_buffers[input_tensor_idx], host_input_buffer,
                        input_bytes,
                        cudaMemcpyHostToDevice, stream);

        // Set tensor addresses for ALL IO tensors
        for (int i = 0; i < num_io_tensors; ++i) {
            auto name = engine->getIOTensorName(i);
            context->setTensorAddress(name, device_buffers[i]);
        }

        if (!input_shape_set) {
            for (int i = 0; i < num_io_tensors; ++i) {
                auto name = engine->getIOTensorName(i);
                if (engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) {
                    nvinfer1::Dims dims;
                    dims.nbDims = 4;
                    dims.d[0] = 1;
                    dims.d[1] = 3;
                    dims.d[2] = input_height;
                    dims.d[3] = input_width;
                    context->setInputShape(name, dims);
                }
            }
            input_shape_set = true;
        }

        // Run inference
        context->enqueueV3(stream);

        if (is_detr) {
            cudaMemcpyAsync(host_outputs[detr_boxes_idx], device_buffers[detr_boxes_idx],
                            output_sizes[detr_boxes_idx] * sizeof(float), cudaMemcpyDeviceToHost, stream);
            cudaMemcpyAsync(host_outputs[detr_logits_idx], device_buffers[detr_logits_idx],
                            output_sizes[detr_logits_idx] * sizeof(float), cudaMemcpyDeviceToHost, stream);
            cudaStreamSynchronize(stream);
            postprocess_detr(original_size, detections[b]);
        } else if (multi_head) {
            // Copy all output tensors back to host
            for (int s = 0; s < NUM_SCALES; ++s) {
                int ri = scales[s].reg_tensor_idx;
                int ci = scales[s].cls_tensor_idx;
                cudaMemcpyAsync(host_outputs[ri], device_buffers[ri],
                                output_sizes[ri] * sizeof(float),
                                cudaMemcpyDeviceToHost, stream);
                cudaMemcpyAsync(host_outputs[ci], device_buffers[ci],
                                output_sizes[ci] * sizeof(float),
                                cudaMemcpyDeviceToHost, stream);
            }
            cudaStreamSynchronize(stream);
            postprocess_multihead(original_size, detections[b]);
        } else {
            // Copy single fused output
            for (int i = 0; i < num_io_tensors; ++i) {
                if (i == input_tensor_idx) continue;
                cudaMemcpyAsync(host_output_fused, device_buffers[i],
                                fused_output_size * sizeof(float),
                                cudaMemcpyDeviceToHost, stream);
                break;
            }
            cudaStreamSynchronize(stream);
            postprocess_fused(host_output_fused, detections[b], original_size);
        }
    }
}

// ──────────────────────── postprocess_detr ──────────────────
void trt_rf_detr_detector::postprocess_detr(
    const cv::Size& original_size, std::vector<Detection>& detections) {

    detections.clear();
    const float* boxes_data = host_outputs[detr_boxes_idx];
    const float* logits_data = host_outputs[detr_logits_idx];

    for (int i = 0; i < detr_num_queries; ++i) {
        float max_score = -1e9f;
        int best_class = 0;
        
        for (int c = 0; c < num_classes; ++c) {
            float score = logits_data[i * num_classes + c];
            if (score > max_score) {
                max_score = score;
                best_class = c;
            }
        }
        max_score = sigmoid(max_score);
        
        if (max_score < conf_threshold) continue;

        float cx = boxes_data[i * 4 + 0];
        float cy = boxes_data[i * 4 + 1];
        float w  = boxes_data[i * 4 + 2];
        float h  = boxes_data[i * 4 + 3];

        // Some DETR models output unnormalized coords (unlikely for TensorRT unless pre-scaled)
        // Assume normalized 0..1 relative to the input_width and input_height:
        cx *= input_width;
        cy *= input_height;
        w *= input_width;
        h *= input_height;

        cx = (cx - letterbox_pad_x) / letterbox_scale;
        cy = (cy - letterbox_pad_y) / letterbox_scale;
        w  = w / letterbox_scale;
        h  = h / letterbox_scale;

        Detection det;
        det.bbox[0] = cx;
        det.bbox[1] = cy;
        det.bbox[2] = w;
        det.bbox[3] = h;
        det.conf = max_score;
        det.class_id = best_class;

        detections.push_back(det);
    }
}

// ──────────────────────── get_rect ──────────────────
cv::Rect get_rect(const cv::Mat& img, const float bbox[4], int input_w, int input_h) {
    float cx = bbox[0];
    float cy = bbox[1];
    float w  = bbox[2];
    float h  = bbox[3];

    int x = static_cast<int>(cx - w / 2);
    int y = static_cast<int>(cy - h / 2);
    int width  = static_cast<int>(w);
    int height = static_cast<int>(h);

    x = std::max(0, x);
    y = std::max(0, y);
    width  = std::min(width,  img.cols - x);
    height = std::min(height, img.rows - y);

    return cv::Rect(x, y, width, height);
}

} // namespace trt_rf_detr
