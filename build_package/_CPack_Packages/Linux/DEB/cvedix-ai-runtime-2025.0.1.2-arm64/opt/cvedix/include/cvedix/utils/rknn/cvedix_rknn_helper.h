#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>
#include "rknn_api.h"

namespace cvedix_utils {
    // RKNN helper class for managing RKNN model inference
    // Wraps RKNN API for easier usage in InstancePipeline
    class cvedix_rknn_helper {
    private:
        rknn_context ctx;
        bool initialized;
        std::string model_path;
        
        // Model info
        rknn_input_output_num io_num;
        std::vector<rknn_tensor_attr> input_attrs;
        std::vector<rknn_tensor_attr> output_attrs;
        
        // Input/output memory
        std::vector<rknn_input> inputs;
        std::vector<rknn_output> outputs;
        std::vector<cv::Mat> input_buffers;
        
        // Helper methods
        int load_model(const std::string& path);
        void release_model();
        int query_model_info();
        
    public:
        cvedix_rknn_helper();
        ~cvedix_rknn_helper();
        
        // Non-copyable
        cvedix_rknn_helper(const cvedix_rknn_helper&) = delete;
        cvedix_rknn_helper& operator=(const cvedix_rknn_helper&) = delete;
        
        // Initialize with model path
        // Returns 0 on success, negative on error
        int init(const std::string& model_path);
        
        // Get model input/output information
        int get_input_num() const { return io_num.n_input; }
        int get_output_num() const { return io_num.n_output; }
        rknn_tensor_attr get_input_attr(int index) const;
        rknn_tensor_attr get_output_attr(int index) const;
        
        // Set input data (from cv::Mat)
        // index: input index (usually 0)
        // mat: input image (BGR format, will be converted if needed)
        // Returns 0 on success
        int set_input(int index, const cv::Mat& mat);
        
        // Run inference
        // Returns 0 on success
        int run();
        
        // Get output data
        // index: output index
        // Returns pointer to output data, nullptr on error
        // Note: memory is managed internally, valid until next run() or set_input()
        float* get_output(int index, int& size);
        
        // Get output as cv::Mat (for convenience)
        // index: output index
        // shape: output shape [batch, height, width, channels] or similar
        cv::Mat get_output_mat(int index, const std::vector<int>& shape);
        
        // Check if initialized
        bool is_initialized() const { return initialized; }
        
        // Get model path
        std::string get_model_path() const { return model_path; }
    };
}
