#pragma once

#ifdef CVEDIX_WITH_RKNN
#include <vector>
#include <deque>
#include <mutex>
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include "cvedix/utils/rknn/cvedix_rknn_helper.h"
#ifdef CVEDIX_WITH_RGA
#include "cvedix/utils/rga/cvedix_rga_helper.h"
#endif

namespace cvedix_nodes {
    class cvedix_rknn_yolov8_detector_node: public cvedix_primary_infer_node
    {
    private:
        std::shared_ptr<cvedix_utils::cvedix_rknn_helper> rknn_helper;
        
#ifdef CVEDIX_WITH_RGA
        std::shared_ptr<cvedix_utils::cvedix_rga_helper> rga_helper;
        bool use_rga = false;
#endif
        
        float score_threshold;
        float nms_threshold;
        int input_width;
        int input_height;
        int num_classes;

        struct letterbox_params {
            float scale = 1.0f;
            int pad_left = 0;
            int pad_top = 0;
            int pad_right = 0;
            int pad_bottom = 0;
            int padded_width = 0;
            int padded_height = 0;
            cv::Size original_size;
        };

        std::deque<letterbox_params> pending_letterbox_params;
        std::mutex letterbox_mutex;

        void sync_model_input_shape();
        
        void parse_yolov8_output(const float* output_data,
                                 int output_size,
                                 const rknn_tensor_attr& output_attr,
                                 int parsed_num_classes,
                                 const letterbox_params& lb_params,
                                 const cv::Size& frame_size,
                                 std::vector<cv::Rect>& boxes,
                                 std::vector<float>& scores,
                                 std::vector<int>& class_ids);
        
    protected:
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;
        
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;
        
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, 
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        cvedix_rknn_yolov8_detector_node(std::string node_name,
                                         std::string model_path,
                                         float score_threshold = 0.5f,
                                         float nms_threshold = 0.5f,
                                         int input_width = 640,
                                         int input_height = 640,
                                         int num_classes = 80,
                                         std::string labels_path = "",
                                         int class_id_offset = 0);
        ~cvedix_rknn_yolov8_detector_node();
    };
}

#endif // CVEDIX_WITH_RKNN

