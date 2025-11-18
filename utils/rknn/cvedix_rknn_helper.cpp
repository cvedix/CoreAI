#ifdef CVEDIX_WITH_RKNN

#include "cvedix_rknn_helper.h"
#include "../logger/cvedix_logger.h"
#include <fstream>
#include <cstring>

namespace cvedix_utils {

    cvedix_rknn_helper::cvedix_rknn_helper() 
        : ctx(0), initialized(false), model_path("") {
        memset(&io_num, 0, sizeof(io_num));
    }

    cvedix_rknn_helper::~cvedix_rknn_helper() {
        release_model();
    }

    int cvedix_rknn_helper::load_model(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] Failed to open model file: %s", path.c_str()));
            return -1;
        }
        
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<char> model_data(size);
        if (!file.read(model_data.data(), size)) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] Failed to read model file: %s", path.c_str()));
            return -1;
        }
        
        int ret = rknn_init(&ctx, model_data.data(), size, 0, nullptr);
        if (ret != RKNN_SUCC) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] rknn_init failed, ret=%d", ret));
            return -1;
        }
        
        return 0;
    }

    void cvedix_rknn_helper::release_model() {
        if (ctx != 0) {
            // Release outputs
            if (outputs.size() > 0) {
                rknn_outputs_release(ctx, io_num.n_output, outputs.data());
            }
            rknn_destroy(ctx);
            ctx = 0;
        }
        initialized = false;
        inputs.clear();
        outputs.clear();
        input_attrs.clear();
        output_attrs.clear();
    }

    int cvedix_rknn_helper::query_model_info() {
        int ret;
        
        // Query input/output number
        ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
        if (ret != RKNN_SUCC) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] rknn_query IN_OUT_NUM failed, ret=%d", ret));
            return -1;
        }
        
        CVEDIX_INFO(cvedix_utils::string_format("[RKNN] Model input num: %d, output num: %d", 
                                                io_num.n_input, io_num.n_output));
        
        // Query input attributes
        input_attrs.resize(io_num.n_input);
        for (int i = 0; i < io_num.n_input; i++) {
            input_attrs[i].index = i;
            ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
            if (ret != RKNN_SUCC) {
                CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] rknn_query INPUT_ATTR[%d] failed, ret=%d", i, ret));
                return -1;
            }
            CVEDIX_INFO(cvedix_utils::string_format("[RKNN] Input[%d]: name=%s, n_dims=%d, dims=[%d,%d,%d,%d], type=%d, fmt=%d, size=%d",
                i, input_attrs[i].name, input_attrs[i].n_dims,
                input_attrs[i].dims[0], input_attrs[i].dims[1], 
                input_attrs[i].dims[2], input_attrs[i].dims[3],
                input_attrs[i].type, input_attrs[i].fmt, input_attrs[i].size));
        }
        
        // Query output attributes
        output_attrs.resize(io_num.n_output);
        for (int i = 0; i < io_num.n_output; i++) {
            output_attrs[i].index = i;
            ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
            if (ret != RKNN_SUCC) {
                CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] rknn_query OUTPUT_ATTR[%d] failed, ret=%d", i, ret));
                return -1;
            }
            CVEDIX_INFO(cvedix_utils::string_format("[RKNN] Output[%d]: name=%s, n_dims=%d, dims=[%d,%d,%d,%d], type=%d, fmt=%d, size=%d",
                i, output_attrs[i].name, output_attrs[i].n_dims,
                output_attrs[i].dims[0], output_attrs[i].dims[1], 
                output_attrs[i].dims[2], output_attrs[i].dims[3],
                output_attrs[i].type, output_attrs[i].fmt, output_attrs[i].size));
        }
        
        // Initialize input/output structures
        inputs.resize(io_num.n_input);
        outputs.resize(io_num.n_output);
        
        return 0;
    }

    int cvedix_rknn_helper::init(const std::string& path) {
        if (initialized) {
            CVEDIX_WARN("[RKNN] Already initialized, releasing previous model");
            release_model();
        }
        
        model_path = path;
        int ret = load_model(path);
        if (ret != 0) {
            return ret;
        }
        
        ret = query_model_info();
        if (ret != 0) {
            release_model();
            return ret;
        }
        
        initialized = true;
        CVEDIX_INFO(cvedix_utils::string_format("[RKNN] Model loaded successfully: %s", path.c_str()));
        return 0;
    }

    rknn_tensor_attr cvedix_rknn_helper::get_input_attr(int index) const {
        if (index >= 0 && index < io_num.n_input) {
            return input_attrs[index];
        }
        rknn_tensor_attr empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }

    rknn_tensor_attr cvedix_rknn_helper::get_output_attr(int index) const {
        if (index >= 0 && index < io_num.n_output) {
            return output_attrs[index];
        }
        rknn_tensor_attr empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }

    int cvedix_rknn_helper::set_input(int index, const cv::Mat& mat) {
        if (!initialized || ctx == 0) {
            CVEDIX_ERROR("[RKNN] Not initialized");
            return -1;
        }
        
        if (index < 0 || index >= io_num.n_input) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] Invalid input index: %d", index));
            return -1;
        }
        
        auto& input_attr = input_attrs[index];
        
        // Prepare input data based on model requirements
        cv::Mat input_mat = mat.clone();
        
        // Convert BGR to RGB if needed (most RKNN models expect RGB)
        if (input_attr.fmt == RKNN_TENSOR_NHWC) {
            if (mat.channels() == 3) {
                cv::cvtColor(input_mat, input_mat, cv::COLOR_BGR2RGB);
            }
        }
        
        // Resize if needed
        int model_w = input_attr.dims[2];  // Usually width
        int model_h = input_attr.dims[1];  // Usually height
        if (input_mat.cols != model_w || input_mat.rows != model_h) {
            cv::resize(input_mat, input_mat, cv::Size(model_w, model_h));
        }
        
        // Normalize if needed (assuming uint8 input)
        if (input_attr.type == RKNN_TENSOR_FLOAT32) {
            input_mat.convertTo(input_mat, CV_32F, 1.0/255.0);
        }
        
        // Prepare input structure
        inputs[index].index = index;
        inputs[index].type = input_attr.type;
        inputs[index].fmt = input_attr.fmt;
        inputs[index].size = input_mat.total() * input_mat.elemSize();
        inputs[index].buf = input_mat.data;
        
        return 0;
    }

    int cvedix_rknn_helper::run() {
        if (!initialized || ctx == 0) {
            CVEDIX_ERROR("[RKNN] Not initialized");
            return -1;
        }
        
        // Release previous outputs
        if (outputs.size() > 0) {
            rknn_outputs_release(ctx, io_num.n_output, outputs.data());
        }
        
        // Set inputs
        int ret = rknn_inputs_set(ctx, io_num.n_input, inputs.data());
        if (ret != RKNN_SUCC) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] rknn_inputs_set failed, ret=%d", ret));
            return -1;
        }
        
        // Run inference
        ret = rknn_run(ctx, nullptr);
        if (ret != RKNN_SUCC) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] rknn_run failed, ret=%d", ret));
            return -1;
        }
        
        // Get outputs
        outputs.resize(io_num.n_output);
        for (int i = 0; i < io_num.n_output; i++) {
            outputs[i].want_float = 1;  // Get float output
        }
        ret = rknn_outputs_get(ctx, io_num.n_output, outputs.data(), nullptr);
        if (ret != RKNN_SUCC) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] rknn_outputs_get failed, ret=%d", ret));
            return -1;
        }
        
        return 0;
    }

    float* cvedix_rknn_helper::get_output(int index, int& size) {
        if (!initialized) {
            CVEDIX_ERROR("[RKNN] Not initialized");
            return nullptr;
        }
        
        if (index < 0 || index >= io_num.n_output) {
            CVEDIX_ERROR(cvedix_utils::string_format("[RKNN] Invalid output index: %d", index));
            return nullptr;
        }
        
        size = outputs[index].size / sizeof(float);
        return (float*)outputs[index].buf;
    }

    cv::Mat cvedix_rknn_helper::get_output_mat(int index, const std::vector<int>& shape) {
        if (!initialized) {
            return cv::Mat();
        }
        
        int size;
        float* data = get_output(index, size);
        if (data == nullptr) {
            return cv::Mat();
        }
        
        // Create Mat from output data
        // Assuming shape is [batch, height, width, channels] or [batch, channels, height, width]
        if (shape.size() == 4) {
            // NCHW or NHWC format
            int total_size = 1;
            for (int s : shape) total_size *= s;
            
            if (total_size == size) {
                // Try NHWC first (common for RKNN)
                return cv::Mat(shape.size(), shape.data(), CV_32F, data);
            }
        }
        
        // Fallback: create 1D Mat
        return cv::Mat(1, size, CV_32F, data);
    }

} // namespace cvedix_utils

#endif // CVEDIX_WITH_RKNN

