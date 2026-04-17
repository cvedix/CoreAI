#include "rknn_yolov26.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>

#ifdef CVEDIX_WITH_RGA
#include <rga/im2d.hpp>
#include <rga/rga.h>
#endif

namespace rknn_yolov26 {

namespace {

inline int clamp_int(float value, int min_value, int max_value) {
    return value > min_value ? (value < max_value ? static_cast<int>(value) : max_value) : min_value;
}

inline float dequant_u8(uint8_t qnt, int32_t zp, float scale) {
    return (static_cast<float>(qnt) - static_cast<float>(zp)) * scale;
}

inline float dequant_i8(int8_t qnt, int32_t zp, float scale) {
    return (static_cast<float>(qnt) - static_cast<float>(zp)) * scale;
}

float fp16_to_fp32(uint16_t value) {
    const uint32_t sign = (value & 0x8000u) << 16;
    uint32_t exponent = (value & 0x7C00u) >> 10;
    uint32_t mantissa = value & 0x03FFu;

    if (exponent == 0) {
        if (mantissa == 0) {
            const uint32_t bits = sign;
            float out;
            std::memcpy(&out, &bits, sizeof(out));
            return out;
        }
        while ((mantissa & 0x0400u) == 0) {
            mantissa <<= 1;
            --exponent;
        }
        ++exponent;
        mantissa &= ~0x0400u;
    } else if (exponent == 31) {
        const uint32_t bits = sign | 0x7F800000u | (mantissa << 13);
        float out;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }

    exponent = exponent + (127 - 15);
    const uint32_t bits = sign | (exponent << 23) | (mantissa << 13);
    float out;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

float read_tensor_value(const rknn_output& output, const rknn_tensor_attr& attr, int index) {
    switch (attr.type) {
        case RKNN_TENSOR_INT8:
            return dequant_i8(reinterpret_cast<int8_t*>(output.buf)[index], attr.zp, attr.scale);
        case RKNN_TENSOR_UINT8:
            return dequant_u8(reinterpret_cast<uint8_t*>(output.buf)[index], attr.zp, attr.scale);
        case RKNN_TENSOR_FLOAT32:
            return reinterpret_cast<float*>(output.buf)[index];
        case RKNN_TENSOR_FLOAT16:
            return fp16_to_fp32(reinterpret_cast<uint16_t*>(output.buf)[index]);
        default:
            return 0.0f;
    }
}

struct ParsedDetection {
    float x1;
    float y1;
    float x2;
    float y2;
    float conf;
    int class_id;
};

float iou_xyxy(const ParsedDetection& a, const ParsedDetection& b) {
    const float x1 = std::max(a.x1, b.x1);
    const float y1 = std::max(a.y1, b.y1);
    const float x2 = std::min(a.x2, b.x2);
    const float y2 = std::min(a.y2, b.y2);
    const float inter_w = std::max(0.0f, x2 - x1);
    const float inter_h = std::max(0.0f, y2 - y1);
    const float inter = inter_w * inter_h;
    const float area_a = std::max(0.0f, a.x2 - a.x1) * std::max(0.0f, a.y2 - a.y1);
    const float area_b = std::max(0.0f, b.x2 - b.x1) * std::max(0.0f, b.y2 - b.y1);
    return inter / (area_a + area_b - inter + 1e-6f);
}

std::vector<int> logical_dims(const rknn_tensor_attr& attr) {
    std::vector<int> dims;
    for (uint32_t i = 0; i < attr.n_dims; ++i) {
        if (attr.dims[i] > 1) {
            dims.push_back(attr.dims[i]);
        }
    }
    if (!dims.empty() && dims.front() == 1) {
        dims.erase(dims.begin());
    }
    return dims;
}

#ifdef CVEDIX_WITH_RGA
int adaptive_letterbox(const cv::Mat& src, int target_size, uint8_t* dst_virtual_addr, letterbox_t* letterbox, uint8_t fill_color) {
    if (src.empty() || dst_virtual_addr == nullptr || target_size <= 0 || letterbox == nullptr) {
        return -1;
    }

    const float scale = std::min(static_cast<float>(target_size) / src.cols,
                                 static_cast<float>(target_size) / src.rows);
    const int new_width = static_cast<int>(src.cols * scale);
    const int new_height = static_cast<int>(src.rows * scale);
    const int pad_left = (target_size - new_width) / 2;
    const int pad_top = (target_size - new_height) / 2;

    std::memset(dst_virtual_addr, fill_color, target_size * target_size * 3);

    rga_buffer_t src_buf = wrapbuffer_virtualaddr(src.data, src.cols, src.rows, RK_FORMAT_RGB_888);
    rga_buffer_t dst_buf = wrapbuffer_virtualaddr(dst_virtual_addr + (pad_top * target_size + pad_left) * 3,
                                                  new_width,
                                                  new_height,
                                                  RK_FORMAT_RGB_888);
    if (imresize(src_buf, dst_buf, scale, scale, INTER_LINEAR) != IM_STATUS_SUCCESS) {
        return -1;
    }

    letterbox->scale = scale;
    letterbox->x_pad = pad_left;
    letterbox->y_pad = pad_top;
    return 0;
}
#else
int adaptive_letterbox(const cv::Mat& src, int target_size, uint8_t* dst_virtual_addr, letterbox_t* letterbox, uint8_t fill_color) {
    if (src.empty() || dst_virtual_addr == nullptr || target_size <= 0 || letterbox == nullptr) {
        return -1;
    }

    const float scale = std::min(static_cast<float>(target_size) / src.cols,
                                 static_cast<float>(target_size) / src.rows);
    const int new_width = static_cast<int>(src.cols * scale);
    const int new_height = static_cast<int>(src.rows * scale);
    const int pad_left = (target_size - new_width) / 2;
    const int pad_top = (target_size - new_height) / 2;

    cv::Mat dst(target_size, target_size, CV_8UC3, cv::Scalar(fill_color, fill_color, fill_color));
    cv::Mat resized;
    cv::resize(src, resized, cv::Size(new_width, new_height));
    resized.copyTo(dst(cv::Rect(pad_left, pad_top, new_width, new_height)));

    std::memcpy(dst_virtual_addr, dst.data, target_size * target_size * 3);
    letterbox->scale = scale;
    letterbox->x_pad = pad_left;
    letterbox->y_pad = pad_top;
    return 0;
}
#endif

int NC1HWC2_i8_to_NCHW_i8(const int8_t* src, int8_t* dst, int* dims, int channel, int h, int w) {
    const int batch = dims[0];
    const int c1 = dims[1];
    const int c2 = dims[4];
    const int hw_src = dims[2] * dims[3];
    const int hw_dst = h * w;

    for (int b = 0; b < batch; ++b) {
        const int8_t* src_b = src + b * c1 * hw_src * c2;
        int8_t* dst_b = dst + b * channel * hw_dst;
        for (int c = 0; c < channel; ++c) {
            const int plane = c / c2;
            const int offset = c % c2;
            const int8_t* src_bc = src_b + plane * hw_src * c2;
            for (int hw = 0; hw < hw_dst; ++hw) {
                dst_b[c * hw_dst + hw] = src_bc[c2 * hw + offset];
            }
        }
    }
    return 0;
}

} // namespace

rknn_yolov26_detector::rknn_yolov26_detector(const std::string& model_path, int num_classes)
    : model_path(model_path), num_classes(num_classes) {}

rknn_yolov26_detector::~rknn_yolov26_detector() {
    if (is_initialized) {
        release_mems();
        release_model();
    }
}

int rknn_yolov26_detector::init() {
    if (is_initialized) {
        return 0;
    }

    int ret = init_model(model_path);
    if (ret != 0) {
        return ret;
    }

    rknn_query(ctxs[0], RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));

    input_attrs.resize(io_num.n_input);
    input_native_attrs.resize(io_num.n_input);
    for (uint32_t i = 0; i < io_num.n_input; ++i) {
        input_attrs[i].index = i;
        input_native_attrs[i].index = i;
        rknn_query(ctxs[0], RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
        rknn_query(ctxs[0], RKNN_QUERY_NATIVE_INPUT_ATTR, &input_native_attrs[i], sizeof(rknn_tensor_attr));
    }

    output_attrs.resize(io_num.n_output);
    output_native_attrs.resize(io_num.n_output);
    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        output_attrs[i].index = i;
        output_native_attrs[i].index = i;
        rknn_query(ctxs[0], RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
        rknn_query(ctxs[0], RKNN_QUERY_NATIVE_OUTPUT_ATTR, &output_native_attrs[i], sizeof(rknn_tensor_attr));
    }

    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
        model_channel = input_attrs[0].dims[1];
        model_height = input_attrs[0].dims[2];
        model_width = input_attrs[0].dims[3];
    } else {
        model_height = input_attrs[0].dims[1];
        model_width = input_attrs[0].dims[2];
        model_channel = input_attrs[0].dims[3];
    }

    initialize_mems();
    is_initialized = true;
    return 0;
}

int rknn_yolov26_detector::init_model(const std::string& model_path) {
    std::ifstream file(model_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return -1;
    }

    const size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), static_cast<std::streamsize>(size))) {
        return -1;
    }

    int ret = rknn_init(&ctxs[0], buffer.data(), size, RKNN_FLAG_COLLECT_PERF_MASK | RKNN_FLAG_ENABLE_SRAM, nullptr);
    if (ret < 0) {
        return ret;
    }

    for (int i = 1; i < 3; ++i) {
        ret = rknn_dup_context(&ctxs[0], &ctxs[i]);
        if (ret < 0) {
            return ret;
        }
    }

    rknn_set_core_mask(ctxs[0], RKNN_NPU_CORE_0);
    rknn_set_core_mask(ctxs[1], RKNN_NPU_CORE_1);
    rknn_set_core_mask(ctxs[2], RKNN_NPU_CORE_2);
    return 0;
}

void rknn_yolov26_detector::initialize_mems() {
    input_mems.resize(3);
    output_mems.resize(3);

    for (int ctx = 0; ctx < 3; ++ctx) {
        input_mems[ctx].resize(io_num.n_input);
        output_mems[ctx].resize(io_num.n_output);

        for (uint32_t i = 0; i < io_num.n_input; ++i) {
            input_native_attrs[i].type = RKNN_TENSOR_UINT8;
            input_mems[ctx][i] = rknn_create_mem(ctxs[ctx], input_native_attrs[i].size_with_stride);
            rknn_set_io_mem(ctxs[ctx], input_mems[ctx][i], &input_native_attrs[i]);
        }

        for (uint32_t i = 0; i < io_num.n_output; ++i) {
            output_mems[ctx][i] = rknn_create_mem(ctxs[ctx], output_native_attrs[i].size_with_stride);
            rknn_set_io_mem(ctxs[ctx], output_mems[ctx][i], &output_native_attrs[i]);
        }
    }
}

void rknn_yolov26_detector::release_model() {
    for (auto& ctx : ctxs) {
        if (ctx) {
            rknn_destroy(ctx);
            ctx = 0;
        }
    }
}

void rknn_yolov26_detector::release_mems() {
    for (int ctx = 0; ctx < 3; ++ctx) {
        for (auto* mem : input_mems[ctx]) {
            if (mem) {
                rknn_destroy_mem(ctxs[ctx], mem);
            }
        }
        for (auto* mem : output_mems[ctx]) {
            if (mem) {
                rknn_destroy_mem(ctxs[ctx], mem);
            }
        }
    }
}

int rknn_yolov26_detector::run_inference(cv::Mat& input_image,
                                         object_detect_result_list* od_results,
                                         float conf_threshold,
                                         float nms_threshold) {
    if (!is_initialized) {
        return -1;
    }

    const int ctx_index = current_ctx_index;
    current_ctx_index = (current_ctx_index + 1) % 3;

    letterbox_t letter_box{};
    if (adaptive_letterbox(input_image,
                           model_width,
                           reinterpret_cast<uint8_t*>(input_mems[ctx_index][0]->virt_addr),
                           &letter_box,
                           114) != 0) {
        return -1;
    }

    int ret = rknn_run(ctxs[ctx_index], nullptr);
    if (ret != 0) {
        return ret;
    }

    std::vector<rknn_output> outputs(io_num.n_output);
    std::vector<void*> output_bufs(io_num.n_output, nullptr);

    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        const int size = output_native_attrs[i].n_elems * output_native_attrs[i].size / std::max(1u, output_native_attrs[i].n_elems);
        output_bufs[i] = std::malloc(output_native_attrs[i].size_with_stride);
        outputs[i].buf = output_bufs[i];
        outputs[i].size = output_native_attrs[i].size_with_stride;

        if (output_native_attrs[i].fmt == RKNN_TENSOR_NC1HWC2) {
            const int channel = output_attrs[i].dims[1];
            const int h = output_attrs[i].n_dims > 2 ? output_attrs[i].dims[2] : 1;
            const int w = output_attrs[i].n_dims > 3 ? output_attrs[i].dims[3] : 1;
            NC1HWC2_i8_to_NCHW_i8(reinterpret_cast<int8_t*>(output_mems[ctx_index][i]->virt_addr),
                                 reinterpret_cast<int8_t*>(outputs[i].buf),
                                 reinterpret_cast<int*>(output_native_attrs[i].dims),
                                 channel,
                                 h,
                                 w);
        } else {
            std::memcpy(outputs[i].buf, output_mems[ctx_index][i]->virt_addr, output_native_attrs[i].size_with_stride);
        }
        (void)size;
    }

    std::memset(od_results, 0, sizeof(object_detect_result_list));
    post_process(input_image, outputs.data(), &letter_box, conf_threshold, nms_threshold, od_results);

    for (void* buffer : output_bufs) {
        std::free(buffer);
    }
    return 0;
}

int rknn_yolov26_detector::post_process(cv::Mat& input_image,
                                        void* outputs_ptr,
                                        letterbox_t* letter_box,
                                        float conf_threshold,
                                        float nms_threshold,
                                        object_detect_result_list* od_results) {
    if (io_num.n_output == 0) {
        return -1;
    }

    auto* outputs = reinterpret_cast<rknn_output*>(outputs_ptr);
    const rknn_tensor_attr& attr = output_attrs[0];
    const std::vector<int> dims = logical_dims(attr);
    if (dims.size() < 2) {
        return -1;
    }

    int rows = 0;
    int cols = 0;
    bool transpose = false;
    bool end2end = false;
    const int dim1 = dims[dims.size() - 2];
    const int dim2 = dims[dims.size() - 1];

    if (dim2 == 6) {
        end2end = true;
        rows = dim1;
        cols = dim2;
    } else if (dim1 == 6) {
        end2end = true;
        rows = dim2;
        cols = dim1;
        transpose = true;
    } else if (dim1 > dim2) {
        rows = dim1;
        cols = dim2;
    } else {
        rows = dim2;
        cols = dim1;
        transpose = true;
    }

    if (!end2end) {
        num_classes = cols - 4;
    }

    auto value_at = [&](int row, int col) {
        const int index = transpose ? (col * rows + row) : (row * cols + col);
        return read_tensor_value(outputs[0], attr, index);
    };

    std::vector<ParsedDetection> parsed;
    parsed.reserve(rows);

    for (int row = 0; row < rows; ++row) {
        ParsedDetection det{};
        if (end2end) {
            det.conf = value_at(row, 4);
            if (det.conf < conf_threshold) {
                continue;
            }
            det.class_id = static_cast<int>(value_at(row, 5));
            det.x1 = value_at(row, 0);
            det.y1 = value_at(row, 1);
            det.x2 = value_at(row, 2);
            det.y2 = value_at(row, 3);
        } else {
            float best_score = 0.0f;
            int best_class = 0;
            for (int cls = 0; cls < num_classes; ++cls) {
                const float score = value_at(row, 4 + cls);
                if (score > best_score) {
                    best_score = score;
                    best_class = cls;
                }
            }
            if (best_score < conf_threshold) {
                continue;
            }

            const float cx = value_at(row, 0);
            const float cy = value_at(row, 1);
            const float w = value_at(row, 2);
            const float h = value_at(row, 3);

            det.conf = best_score;
            det.class_id = best_class;
            det.x1 = cx - w * 0.5f;
            det.y1 = cy - h * 0.5f;
            det.x2 = cx + w * 0.5f;
            det.y2 = cy + h * 0.5f;
        }

        det.x1 = (det.x1 - letter_box->x_pad) / letter_box->scale;
        det.y1 = (det.y1 - letter_box->y_pad) / letter_box->scale;
        det.x2 = (det.x2 - letter_box->x_pad) / letter_box->scale;
        det.y2 = (det.y2 - letter_box->y_pad) / letter_box->scale;

        det.x1 = std::max(0.0f, std::min(det.x1, static_cast<float>(input_image.cols)));
        det.y1 = std::max(0.0f, std::min(det.y1, static_cast<float>(input_image.rows)));
        det.x2 = std::max(0.0f, std::min(det.x2, static_cast<float>(input_image.cols)));
        det.y2 = std::max(0.0f, std::min(det.y2, static_cast<float>(input_image.rows)));
        parsed.push_back(det);
    }

    if (!end2end) {
        std::sort(parsed.begin(), parsed.end(), [](const ParsedDetection& a, const ParsedDetection& b) {
            return a.conf > b.conf;
        });
        std::vector<bool> suppressed(parsed.size(), false);
        std::vector<ParsedDetection> kept;

        for (size_t i = 0; i < parsed.size(); ++i) {
            if (suppressed[i]) {
                continue;
            }
            kept.push_back(parsed[i]);
            for (size_t j = i + 1; j < parsed.size(); ++j) {
                if (suppressed[j] || parsed[i].class_id != parsed[j].class_id) {
                    continue;
                }
                if (iou_xyxy(parsed[i], parsed[j]) > nms_threshold) {
                    suppressed[j] = true;
                }
            }
        }

        parsed = std::move(kept);
    }

    od_results->count = std::min(static_cast<int>(parsed.size()), OBJ_NUMB_MAX_SIZE);
    for (int i = 0; i < od_results->count; ++i) {
        od_results->results[i].box.left = clamp_int(parsed[i].x1, 0, input_image.cols);
        od_results->results[i].box.top = clamp_int(parsed[i].y1, 0, input_image.rows);
        od_results->results[i].box.right = clamp_int(parsed[i].x2, 0, input_image.cols);
        od_results->results[i].box.bottom = clamp_int(parsed[i].y2, 0, input_image.rows);
        od_results->results[i].prop = parsed[i].conf;
        od_results->results[i].cls_id = parsed[i].class_id;
    }

    return 0;
}

void rknn_yolov26_detector::query_model_info() {
}

} // namespace rknn_yolov26