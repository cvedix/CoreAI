/**
 * @file cvedix_paddle_detector_node.h
 * @brief PaddleDetection object detector node (CPU/GPU)
 */

#pragma once

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"

#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "include/object_detector.h"

namespace cvedix_nodes {

/**
 * @class cvedix_paddle_detector_node
 * @brief Object detector node using PaddleDetection inference API
 */
class cvedix_paddle_detector_node : public cvedix_primary_infer_node {
public:
    cvedix_paddle_detector_node(
        const std::string& node_name,
        const std::string& model_dir,
        const std::string& labels_path = "",
        bool use_gpu = false,
        int gpu_id = 0,
        const std::string& run_mode = "paddle",
        bool use_mkldnn = false,
        int cpu_threads = 1,
        int batch_size = 1,
        float conf_threshold = 0.5f,
        int class_id_offset = 0,
        int trt_min_shape = 1,
        int trt_max_shape = 1280,
        int trt_opt_shape = 640,
        bool trt_calib_mode = false);

    ~cvedix_paddle_detector_node() override;

    void set_conf_threshold(float thresh);

    void add_allowed_class(int class_id) {
        allowed_class_ids.insert(class_id);
    }

    void clear_allowed_classes() {
        allowed_class_ids.clear();
    }

    void set_allowed_classes(const std::initializer_list<int>& class_ids) {
        allowed_class_ids.clear();
        for (int id : class_ids) {
            allowed_class_ids.insert(id);
        }
    }

    std::string get_label(int class_id) const;

protected:
    void run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

private:
    std::unique_ptr<PaddleDetection::ObjectDetector> detector_;
    float conf_threshold;
    std::set<int> allowed_class_ids;
};

}  // namespace cvedix_nodes
