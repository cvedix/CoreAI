/**
 * @file cvedix_paddle_attribute_node.h
 * @brief Paddle Inference node for general-purpose attribute recognition (PP-LCNet)
 */

#pragma once

#include "base/cvedix_secondary_infer_node.h"

#include <memory>
#include <string>
#include <vector>

namespace paddle_infer {
class Predictor;
}

namespace cvedix_nodes {

/**
 * @class cvedix_paddle_attribute_node
 * @brief Secondary inference node for general-purpose attribute recognition
 *
 * Runs Paddle Inference on cropped target ROIs and appends attribute labels
 * to secondary_* fields on each target. Supports grouped attributes via
 * attribute_group_sizes.
 */
class cvedix_paddle_attribute_node : public cvedix_secondary_infer_node {
public:
    cvedix_paddle_attribute_node(
        const std::string& node_name,
        const std::string& model_dir,
        const std::string& labels_path = "",
        bool use_gpu = false,
        int gpu_id = 0,
        const std::string& run_mode = "paddle",
        bool use_mkldnn = false,
        int cpu_threads = 1,
        int input_width = 256,
        int input_height = 192,
        int batch_size = 1,
        float score_threshold = 0.5f,
        const std::vector<int>& attribute_group_sizes = {},
        std::vector<int> p_class_ids_applied_to = std::vector<int>(),
        int min_width_applied_to = 0,
        int min_height_applied_to = 0,
        int crop_padding = 10,
        bool apply_sigmoid = false,
        float scale = 1.0f / 255.0f,
        cv::Scalar mean = cv::Scalar(0.485f, 0.456f, 0.406f),
        cv::Scalar std = cv::Scalar(0.229f, 0.224f, 0.225f),
        bool swap_rb = true,
        bool swap_chn = false,
        int trt_min_shape = 1,
        int trt_max_shape = 1280,
        int trt_opt_shape = 640,
        bool trt_calib_mode = false);

    ~cvedix_paddle_attribute_node() override;

    void set_score_threshold(float threshold);
    void set_attribute_group_sizes(const std::vector<int>& sizes);

protected:
    void run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

private:
    void init_predictor();
    void preprocess_batch(const std::vector<cv::Mat>& mats, cv::Mat& blob) const;
    void append_attribute_top1(
        const float* scores,
        int dim,
        int label_offset,
        cvedix_objects::cvedix_frame_target& target) const;
    void append_attribute_grouped(
        const float* scores,
        int dim,
        int label_offset,
        cvedix_objects::cvedix_frame_target& target) const;
    void append_attribute_multilabel(
        const float* scores,
        int dim,
        cvedix_objects::cvedix_frame_target& target) const;

    std::string get_label(int class_id) const;

    std::string model_dir_;
    std::shared_ptr<paddle_infer::Predictor> predictor_;
    float score_threshold_;
    std::vector<int> attribute_group_sizes_;
    bool apply_sigmoid_;

    bool use_gpu_;
    int gpu_id_;
    std::string run_mode_;
    bool use_mkldnn_;
    int cpu_threads_;
    int trt_min_shape_;
    int trt_max_shape_;
    int trt_opt_shape_;
    bool trt_calib_mode_;
};

}  // namespace cvedix_nodes
