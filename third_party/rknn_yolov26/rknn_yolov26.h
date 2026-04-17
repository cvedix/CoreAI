/**
 * @file rknn_yolov26.h
 * @brief RKNN YOLOv26 detector implementation
 */

#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <rknn_api.h>

namespace rknn_yolov26 {

#define OBJ_NAME_MAX_SIZE 64
#define OBJ_NUMB_MAX_SIZE 128

struct letterbox_t {
    int x_pad;
    int y_pad;
    float scale;
};

struct image_rect_t {
    int left;
    int top;
    int right;
    int bottom;
};

struct object_detect_result {
    image_rect_t box;
    float prop;
    int cls_id;
};

struct object_detect_result_list {
    int id;
    int count;
    object_detect_result results[OBJ_NUMB_MAX_SIZE];
};

class rknn_yolov26_detector {
public:
    rknn_yolov26_detector(const std::string& model_path, int num_classes = 80);
    ~rknn_yolov26_detector();

    int init();
    int run_inference(cv::Mat& input_image,
                      object_detect_result_list* od_results,
                      float conf_threshold,
                      float nms_threshold);
    void query_model_info();

    int get_input_width() const { return model_width; }
    int get_input_height() const { return model_height; }

private:
    std::string model_path;
    rknn_context ctxs[3]{};
    bool is_initialized = false;

    std::vector<rknn_tensor_attr> input_attrs;
    std::vector<rknn_tensor_attr> input_native_attrs;
    std::vector<rknn_tensor_attr> output_attrs;
    std::vector<rknn_tensor_attr> output_native_attrs;
    rknn_input_output_num io_num{};

    int model_height = 640;
    int model_width = 640;
    int model_channel = 3;
    int num_classes;

    std::vector<std::vector<rknn_tensor_mem*>> input_mems;
    std::vector<std::vector<rknn_tensor_mem*>> output_mems;

    int init_model(const std::string& model_path);
    void release_model();
    void initialize_mems();
    void release_mems();
    int post_process(cv::Mat& input_image,
                     void* outputs,
                     letterbox_t* letter_box,
                     float conf_threshold,
                     float nms_threshold,
                     object_detect_result_list* od_results);

    int current_ctx_index = 0;
};

} // namespace rknn_yolov26