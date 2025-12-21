/**
 * @file cvedix_lane_detector_node.h
 * @brief Lane detection using CenterNet
 * 
 * Detects lane lines for ADAS/autonomous driving applications.
 * 
 * @see cvedix_primary_infer_node Base class
 */

#pragma once

#include "base/cvedix_primary_infer_node.h"


namespace cvedix_nodes {
    /**
     * @brief Lane line detector (CenterNet-based)
     */
    class cvedix_lane_detector_node: public cvedix_primary_infer_node
    {

    private:
        /* data */
    protected:
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_lane_detector_node(std::string node_name, 
                            std::string model_path, 
                            std::string model_config_path = "", 
                            std::string labels_path = "", 
                            int input_width = 736, 
                            int input_height = 416, 
                            int batch_size = 1,
                            int class_id_offset = 0,
                            float scale = 1,
                            cv::Scalar mean = cv::Scalar(0),
                            cv::Scalar std = cv::Scalar(1),
                            bool swap_rb = true,
                            bool swap_chn = true);
        ~cvedix_lane_detector_node();
    };
}