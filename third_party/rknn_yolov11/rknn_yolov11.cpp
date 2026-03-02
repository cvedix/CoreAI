#include "rknn_yolov11.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <set>
#include <chrono>
#include <iomanip>

#ifdef CVEDIX_WITH_RGA
#include <rga/im2d.hpp>
#include <rga/rga.h>
#endif

namespace rknn_yolov11 {

    // --- Common Helper Functions ---

    inline int32_t __clip(float val, float min, float max) {
        return (int32_t)(val <= min ? min : (val >= max ? max : val));
    }

    inline int8_t qnt_f32_to_affine(float f32, int32_t zp, float scale) {
        float dst_val = (f32 / scale) + zp;
        return (int8_t)__clip(dst_val, -128.0f, 127.0f);
    }

    inline float deqnt_affine_to_f32(int8_t qnt, int32_t zp, float scale) { 
        return ((float)qnt - (float)zp) * scale; 
    }

    inline int clamp(float val, int min, int max) { 
        return val > min ? (val < max ? val : max) : min; 
    }

    static void compute_dfl(float* tensor, int dfl_len, float* box) {
        for (int b = 0; b < 4; b++) {
            float exp_sum = 0.0f;
            float acc_sum = 0.0f;
            for (int i = 0; i < dfl_len; i++) {
                float exp_val = exp(tensor[i + b * dfl_len]);
                exp_sum += exp_val;
                acc_sum += exp_val * i;
            }
            box[b] = acc_sum / exp_sum;
        }
    }

    inline float CalculateOverlap(float xmin0, float ymin0, float xmax0, float ymax0, float xmin1, float ymin1, float xmax1, float ymax1) {
        float w = fmax(0.f, fmin(xmax0, xmax1) - fmax(xmin0, xmin1) + 1.0);
        float h = fmax(0.f, fmin(ymax0, ymax1) - fmax(ymin0, ymin1) + 1.0);
        float i = w * h;
        float u = (xmax0 - xmin0 + 1.0) * (ymax0 - ymin0 + 1.0) + (xmax1 - xmin1 + 1.0) * (ymax1 - ymin1 + 1.0) - i;
        return u <= 0.f ? 0.f : (i / u);
    }

    static int nms(int validCount, std::vector<float>& outputLocations, std::vector<int> classIds, std::vector<int>& order, int filterId, float threshold) {
        for (int i = 0; i < validCount; ++i) {
            int n = order[i];
            if (n == -1 || classIds[n] != filterId) {
                continue;
            }
            float xmin0 = outputLocations[n * 4 + 0];
            float ymin0 = outputLocations[n * 4 + 1];
            float xmax0 = outputLocations[n * 4 + 0] + outputLocations[n * 4 + 2];
            float ymax0 = outputLocations[n * 4 + 1] + outputLocations[n * 4 + 3];
            
            for (int j = i + 1; j < validCount; ++j) {
                int m = order[j];
                if (m == -1 || classIds[m] != filterId) {
                    continue;
                }
                float xmin1 = outputLocations[m * 4 + 0];
                float ymin1 = outputLocations[m * 4 + 1];
                float xmax1 = outputLocations[m * 4 + 0] + outputLocations[m * 4 + 2];
                float ymax1 = outputLocations[m * 4 + 1] + outputLocations[m * 4 + 3];
                float iou = CalculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);
                if (iou > threshold) {
                    order[j] = -1;
                }
            }
        }
        return 0;
    }

    static void quick_sort_indice_inverse(std::vector<float>& input, std::vector<int>& indices) {
        size_t n = input.size();
        if (indices.empty()) {
            indices.resize(n);
            std::iota(indices.begin(), indices.end(), 0);
        }
        std::vector<std::pair<float, int>> paired(n);
        for (size_t i = 0; i < n; ++i) {
            paired[i] = std::make_pair(input[i], indices[i]);
        }
        std::sort(paired.begin(), paired.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
            return a.first > b.first;
        });
        for (size_t i = 0; i < n; ++i) {
            input[i] = paired[i].first;
            indices[i] = paired[i].second;
        }
    }

    static int process_i8(int8_t* box_tensor, int32_t box_zp, float box_scale,
                          int8_t* score_tensor, int32_t score_zp, float score_scale,
                          int8_t* score_sum_tensor, int32_t score_sum_zp, float score_sum_scale,
                          int grid_h, int grid_w, int stride, int dfl_len,
                          std::vector<float>& boxes, std::vector<float>& objProbs, std::vector<int>& classId,
                          float threshold, int num_classes) {
        int validCount = 0;
        int grid_len = grid_h * grid_w;
        int8_t score_thres_i8 = qnt_f32_to_affine(threshold, score_zp, score_scale);
        int8_t score_sum_thres_i8 = qnt_f32_to_affine(threshold, score_sum_zp, score_sum_scale);

        for (int i = 0; i < grid_h; i++) {
            for (int j = 0; j < grid_w; j++) {
                int offset = i * grid_w + j;
                int max_class_id = -1;

                if ((score_sum_tensor != nullptr) && (score_sum_tensor[offset] < score_sum_thres_i8)) {
                    continue;
                }

                int8_t max_score = -score_zp;
                for (int c = 0; c < num_classes; c++) {
                    if ((score_tensor[offset] > score_thres_i8) && (score_tensor[offset] > max_score)) {
                        max_score = score_tensor[offset];
                        max_class_id = c;
                    }
                    offset += grid_len;
                }

                if (max_score > score_thres_i8) {
                    offset = i * grid_w + j;
                    float box[4];
                    std::vector<float> before_dfl(dfl_len * 4);

                    for (int k = 0; k < dfl_len * 4; k++) {
                        before_dfl[k] = deqnt_affine_to_f32(box_tensor[offset], box_zp, box_scale);
                        offset += grid_len;
                    }
                    compute_dfl(before_dfl.data(), dfl_len, box);

                    float x1, y1, x2, y2, w, h;
                    x1 = (-box[0] + j + 0.5) * stride;
                    y1 = (-box[1] + i + 0.5) * stride;
                    x2 = (box[2] + j + 0.5) * stride;
                    y2 = (box[3] + i + 0.5) * stride;
                    w = x2 - x1;
                    h = y2 - y1;

                    boxes.push_back(x1);
                    boxes.push_back(y1);
                    boxes.push_back(w);
                    boxes.push_back(h);
                    objProbs.push_back(deqnt_affine_to_f32(max_score, score_zp, score_scale));
                    classId.push_back(max_class_id);
                    validCount++;
                }
            }
        }
        return validCount;
    }

    static int NC1HWC2_i8_to_NCHW_i8(const int8_t* src, int8_t* dst, int* dims, int channel, int h, int w, int zp, float scale) {
        int batch = dims[0];
        int C1 = dims[1];
        int C2 = dims[4];
        int hw_src = dims[2] * dims[3];
        int hw_dst = h * w;

        for (int i = 0; i < batch; i++) {
            const int8_t* src_b = src + i * C1 * hw_src * C2;
            int8_t* dst_b = dst + i * channel * hw_dst;
            for (int c = 0; c < channel; ++c) {
                int plane = c / C2;
                const int8_t* src_bc = src_b + plane * hw_src * C2;
                int offset = c % C2;
                for (int cur_hw = 0; cur_hw < hw_dst; ++cur_hw) {
                    dst_b[c * hw_dst + cur_hw] = src_bc[C2 * cur_hw + offset];
                }
            }
        }
        return 0;
    }

    // --- RGA Utils ---
#ifdef CVEDIX_WITH_RGA
    static int adaptive_letterbox(const cv::Mat& src, int target_size, uint8_t* dst_virtual_addr, letterbox_t* letterbox, uint8_t fill_color, int interpolation = INTER_LINEAR) {
        if (src.empty() || dst_virtual_addr == nullptr || target_size <= 0 || letterbox == nullptr) {
            return -1;
        }
        int src_width = src.cols;
        int src_height = src.rows;

        if (src_width == target_size && src_height == target_size) {
            memcpy(dst_virtual_addr, src.data, target_size * target_size * 3);
            letterbox->scale = 1.0;
            letterbox->x_pad = 0;
            letterbox->y_pad = 0;
            return 0;
        }

        float scale = std::min(static_cast<float>(target_size) / src_width, static_cast<float>(target_size) / src_height);
        int new_width = static_cast<int>(src_width * scale);
        int new_height = static_cast<int>(src_height * scale);

        if (new_width == target_size && new_height == target_size) {
            rga_buffer_t src_buf = wrapbuffer_virtualaddr(src.data, src_width, src_height, RK_FORMAT_RGB_888);
            rga_buffer_t dst_buf = wrapbuffer_virtualaddr(dst_virtual_addr, target_size, target_size, RK_FORMAT_RGB_888);
            IM_STATUS status = imresize(src_buf, dst_buf, scale, scale, interpolation);
            if (status != IM_STATUS_SUCCESS) return -1;
            letterbox->scale = scale;
            letterbox->x_pad = 0;
            letterbox->y_pad = 0;
            return 0;
        }

        int pad_left = (target_size - new_width) / 2;
        int pad_top = (target_size - new_height) / 2;

        memset(dst_virtual_addr, fill_color, target_size * target_size * 3);

        rga_buffer_t src_buf = wrapbuffer_virtualaddr(src.data, src_width, src_height, RK_FORMAT_RGB_888);
        rga_buffer_t dst_buf = wrapbuffer_virtualaddr(dst_virtual_addr + (pad_top * target_size + pad_left) * 3, new_width, new_height, RK_FORMAT_RGB_888);

        IM_STATUS status = imresize(src_buf, dst_buf, scale, scale, interpolation);
        if (status != IM_STATUS_SUCCESS) return -1;

        letterbox->scale = scale;
        letterbox->x_pad = pad_left;
        letterbox->y_pad = pad_top;
        return 0;
    }
#else
    static int adaptive_letterbox(const cv::Mat& src, int target_size, uint8_t* dst_virtual_addr, letterbox_t* letterbox, uint8_t fill_color, int interpolation = cv::INTER_LINEAR) {
         if (src.empty() || dst_virtual_addr == nullptr || target_size <= 0) return -1;

        int src_width = src.cols;
        int src_height = src.rows;

        if (src_width == target_size && src_height == target_size) {
            memcpy(dst_virtual_addr, src.data, target_size * target_size * 3);
            letterbox->scale = 1.0; letterbox->x_pad = 0; letterbox->y_pad = 0;
            return 0;
        }

        float scale = std::min(static_cast<float>(target_size) / src_width, static_cast<float>(target_size) / src_height);
        int new_width = static_cast<int>(src_width * scale);
        int new_height = static_cast<int>(src_height * scale);

        int pad_left = (target_size - new_width) / 2;
        int pad_top = (target_size - new_height) / 2;

        cv::Mat dst_img(target_size, target_size, CV_8UC3, cv::Scalar(fill_color, fill_color, fill_color));
        cv::Mat resized_img;
        cv::resize(src, resized_img, cv::Size(new_width, new_height), 0, 0, interpolation);
        resized_img.copyTo(dst_img(cv::Rect(pad_left, pad_top, new_width, new_height)));

        memcpy(dst_virtual_addr, dst_img.data, target_size * target_size * 3);

        letterbox->scale = scale;
        letterbox->x_pad = pad_left;
        letterbox->y_pad = pad_top;
        return 0;
    }
#endif

    static void crop_image_to_square_and_16_alignment(cv::Mat& image) {
        // This function is intentionally empty to avoid cropping.
        // We want to detect on the full frame.
        // If specific alignment is needed by RGA/RKNN, adaptive_letterbox usually handles it
        // or the driver handles it.
    }


    // --- Class Implementation ---

    rknn_yolov11_detector::rknn_yolov11_detector(const std::string& model_path, int num_classes) 
        : model_path(model_path), num_classes(num_classes) {}

    rknn_yolov11_detector::~rknn_yolov11_detector() {
        if (is_initialized) {
            release_mems();
            release_model();
        }
    }

    int rknn_yolov11_detector::init() {
        if (is_initialized) return 0;
        int ret = init_model(model_path);
        if (ret != 0) return ret;
        
        int ctx_index = 0;
        rknn_query(ctxs[ctx_index], RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));

        input_attrs.resize(io_num.n_input);
        input_native_attrs.resize(io_num.n_input);
        for (uint32_t i = 0; i < io_num.n_input; ++i) {
            input_attrs[i].index = i;
            rknn_query(ctxs[ctx_index], RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
            input_native_attrs[i].index = i;
            rknn_query(ctxs[ctx_index], RKNN_QUERY_NATIVE_INPUT_ATTR, &input_native_attrs[i], sizeof(rknn_tensor_attr));
        }

        output_attrs.resize(io_num.n_output);
        output_native_attrs.resize(io_num.n_output);
        for (uint32_t i = 0; i < io_num.n_output; ++i) {
            output_attrs[i].index = i;
            rknn_query(ctxs[ctx_index], RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
            output_native_attrs[i].index = i;
            rknn_query(ctxs[ctx_index], RKNN_QUERY_NATIVE_OUTPUT_ATTR, &output_native_attrs[i], sizeof(rknn_tensor_attr));
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
        num_outputs = io_num.n_output;
        
        initialize_mems();
        is_initialized = true;
        return 0;
    }

    int rknn_yolov11_detector::init_model(const std::string& model_path) {
        std::ifstream file(model_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return -1;
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) return -1;

        int ret = rknn_init(&ctxs[0], buffer.data(), size, RKNN_FLAG_COLLECT_PERF_MASK | RKNN_FLAG_ENABLE_SRAM, NULL);
        if (ret < 0) return ret;

        for (int i = 1; i < 3; ++i) {
            ret = rknn_dup_context(&ctxs[0], &ctxs[i]);
            if (ret < 0) return ret;
        }
        
        rknn_set_core_mask(ctxs[0], RKNN_NPU_CORE_0);
        rknn_set_core_mask(ctxs[1], RKNN_NPU_CORE_1);
        rknn_set_core_mask(ctxs[2], RKNN_NPU_CORE_2);
        return 0;
    }

    void rknn_yolov11_detector::initialize_mems() {
        int ctx_size = 3;
        input_mems.resize(ctx_size);
        output_mems.resize(ctx_size);

        for (int i = 0; i < ctx_size; ++i) {
            input_mems[i].resize(io_num.n_input);
            output_mems[i].resize(io_num.n_output);

            for (uint32_t j = 0; j < io_num.n_input; ++j) {
                input_native_attrs[j].type = RKNN_TENSOR_UINT8;
                input_mems[i][j] = rknn_create_mem(ctxs[i], input_native_attrs[j].size_with_stride);
                rknn_set_io_mem(ctxs[i], input_mems[i][j], &input_native_attrs[j]);
            }

            for (uint32_t j = 0; j < io_num.n_output; ++j) {
                output_mems[i][j] = rknn_create_mem(ctxs[i], output_native_attrs[j].size_with_stride);
                rknn_set_io_mem(ctxs[i], output_mems[i][j], &output_native_attrs[j]);
            }
        }
    }

    void rknn_yolov11_detector::release_model() {
        for (int i = 0; i < 3; ++i) {
            rknn_destroy(ctxs[i]);
        }
    }

    void rknn_yolov11_detector::release_mems() {
        int ctx_size = 3;
        for (int i = 0; i < ctx_size; ++i) {
            for (auto mem : input_mems[i]) if (mem) rknn_destroy_mem(ctxs[i], mem);
            for (auto mem : output_mems[i]) if (mem) rknn_destroy_mem(ctxs[i], mem);
        }
    }

    int rknn_yolov11_detector::run_inference(cv::Mat& input_image, object_detect_result_list* od_results, float conf_threshold, float nms_threshold) {
        if (!is_initialized) return -1;
        
        int ctx_index = current_ctx_index;
        current_ctx_index = (current_ctx_index + 1) % 3;

        crop_image_to_square_and_16_alignment(input_image);
        
        letterbox_t letter_box;
        memset(&letter_box, 0, sizeof(letterbox_t));
        uint8_t fill_color = 114;

        int ret = adaptive_letterbox(input_image, 640, (uint8_t*)input_mems[ctx_index][0]->virt_addr, &letter_box, fill_color);
        if (ret != 0) return -1;

        ret = rknn_run(ctxs[ctx_index], NULL);
        if (ret != 0) return ret;

        std::vector<rknn_output> outputs(io_num.n_output);
        std::vector<void*> output_bufs(io_num.n_output);

        for (uint32_t i = 0; i < io_num.n_output; ++i) {
            int channel = output_attrs[i].dims[1];
            int h = output_attrs[i].n_dims > 2 ? output_attrs[i].dims[2] : 1;
            int w = output_attrs[i].n_dims > 3 ? output_attrs[i].dims[3] : 1;
            int zp = output_native_attrs[i].zp;
            float scale = output_native_attrs[i].scale;

            int size = output_native_attrs[i].n_elems * sizeof(int8_t);
            output_bufs[i] = malloc(size);
            outputs[i].buf = output_bufs[i];
            outputs[i].size = size;

            if (output_native_attrs[i].fmt == RKNN_TENSOR_NC1HWC2) {
                NC1HWC2_i8_to_NCHW_i8((int8_t*)output_mems[ctx_index][i]->virt_addr, (int8_t*)outputs[i].buf,
                    (int*)output_native_attrs[i].dims, channel, h, w, zp, scale);
            } else {
                memcpy(outputs[i].buf, output_mems[ctx_index][i]->virt_addr, size);
            }
        }

        memset(od_results, 0, sizeof(object_detect_result_list));
        post_process(input_image, outputs.data(), &letter_box, conf_threshold, nms_threshold, od_results);

        for (auto buf : output_bufs) free(buf);
        return 0;
    }

    int rknn_yolov11_detector::post_process(cv::Mat& input_image, void* outputs, letterbox_t* letter_box, float conf_threshold, float nms_threshold, object_detect_result_list* od_results) {
        rknn_output* _outputs = (rknn_output*)outputs;
        std::vector<float> filterBoxes;
        std::vector<float> objProbs;
        std::vector<int> classId;
        int validCount = 0;

        int dfl_len = output_attrs[0].dims[1] / 4;
        int output_per_branch = io_num.n_output / 3;

        for (int i = 0; i < 3; i++) {
            void* score_sum = nullptr;
            int32_t score_sum_zp = 0;
            float score_sum_scale = 1.0;
            if (output_per_branch == 3) {
                score_sum = _outputs[i * output_per_branch + 2].buf;
                score_sum_zp = output_attrs[i * output_per_branch + 2].zp;
                score_sum_scale = output_attrs[i * output_per_branch + 2].scale;
            }
            int box_idx = i * output_per_branch;
            int score_idx = i * output_per_branch + 1;
            int grid_h = output_attrs[box_idx].dims[2];
            int grid_w = output_attrs[box_idx].dims[3];
            int stride = model_height / grid_h;

            validCount += process_i8((int8_t*)_outputs[box_idx].buf, output_attrs[box_idx].zp, output_attrs[box_idx].scale,
                (int8_t*)_outputs[score_idx].buf, output_attrs[score_idx].zp, output_attrs[score_idx].scale,
                (int8_t*)score_sum, score_sum_zp, score_sum_scale,
                grid_h, grid_w, stride, dfl_len,
                filterBoxes, objProbs, classId, conf_threshold, num_classes);
        }

        if (validCount <= 0) return 0;

        std::vector<int> indexArray(validCount);
        quick_sort_indice_inverse(objProbs, indexArray);

        std::set<int> class_set(classId.begin(), classId.end());
        for (int c : class_set) {
            nms(validCount, filterBoxes, classId, indexArray, c, nms_threshold);
        }

        int last_count = 0;
        for (int i = 0; i < validCount; ++i) {
            if (indexArray[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE) continue;
            int n = indexArray[i];
            float x1 = (filterBoxes[n * 4 + 0] - letter_box->x_pad) / letter_box->scale;
            float y1 = (filterBoxes[n * 4 + 1] - letter_box->y_pad) / letter_box->scale;
            float x2 = x1 + filterBoxes[n * 4 + 2] / letter_box->scale;
            float y2 = y1 + filterBoxes[n * 4 + 3] / letter_box->scale;
            int id = classId[n];
            float obj_conf = objProbs[i];

            od_results->results[last_count].box.left = (int)(clamp(x1, 0, input_image.cols));
            od_results->results[last_count].box.top = (int)(clamp(y1, 0, input_image.rows));
            od_results->results[last_count].box.right = (int)(clamp(x2, 0, input_image.cols));
            od_results->results[last_count].box.bottom = (int)(clamp(y2, 0, input_image.rows));
            od_results->results[last_count].prop = obj_conf;
            od_results->results[last_count].cls_id = id;
            last_count++;
        }
        od_results->count = last_count;
        return 0;
    }
    
    void rknn_yolov11_detector::query_model_info() {
        // Implementation omitted for brevity, can be added if needed
    }

} // namespace rknn_yolov11

