
#pragma once

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"



namespace cvedix_nodes {
    // image segmentation based on Mask RCNN
    // https://github.com/matterport/Mask_RCNN
    class cvedix_mask_rcnn_detector_node: public cvedix_primary_infer_node
    {
    private:
        /* data */
        // names of output layers in mask_rcnn
        const std::vector<std::string> out_names = {"detection_out_final", "detection_masks"};
        float score_threshold = 0.5;
    protected:
        // override infer and preprocess as mask_rcnn has a different logic
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;

        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_mask_rcnn_detector_node(std::string node_name, 
                            std::string model_path, 
                            std::string model_config_path = "", 
                            std::string labels_path = "", 
                            int input_width = 416, 
                            int input_height = 416, 
                            int batch_size = 1,
                            int class_id_offset = 0,
                            float score_threshold = 0.5,
                            float scale = 1 / 255.0,
                            cv::Scalar mean = cv::Scalar(0),
                            cv::Scalar std = cv::Scalar(1),
                            bool swap_rb = true);
        ~cvedix_mask_rcnn_detector_node();
    };    
}