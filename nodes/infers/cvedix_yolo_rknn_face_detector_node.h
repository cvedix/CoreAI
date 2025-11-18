#pragma once

#ifdef CVEDIX_WITH_RKNN
#include "../cvedix_primary_infer_node.h"
#include "../../objects/cvedix_frame_face_target.h"
#include "../../utils/rknn/cvedix_rknn_helper.h"
#ifdef CVEDIX_WITH_RGA
#include "../../utils/rga/cvedix_rga_helper.h"
#endif

namespace cvedix_nodes {
    // Face detector based on YoloV8 running on Rockchip NPU (RKNN)
    // Optimized for RK3566, RK3568, RK3588 chips
    // Uses RGA for hardware-accelerated preprocessing when available
    class cvedix_yolo_rknn_face_detector_node: public cvedix_primary_infer_node
    {
    private:
        std::shared_ptr<cvedix_utils::cvedix_rknn_helper> rknn_helper;
        
#ifdef CVEDIX_WITH_RGA
        std::shared_ptr<cvedix_utils::cvedix_rga_helper> rga_helper;
        bool use_rga;
#endif
        
        float score_threshold;
        float nms_threshold;
        int input_width;
        int input_height;
        
        // YoloV8 output parsing
        void parse_yolov8_output(float* output_data, int output_size, 
                                const cv::Size& frame_size,
                                std::vector<cv::Rect>& boxes,
                                std::vector<float>& scores);
        
    protected:
        // Override preprocess to use RGA if available
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;
        
        // Override infer to use RKNN API
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;
        
        // Override postprocess to parse YoloV8 output and create face targets
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, 
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        cvedix_yolo_rknn_face_detector_node(std::string node_name, 
                                           std::string model_path,  // .rknn file path
                                           float score_threshold = 0.5,
                                           float nms_threshold = 0.5,
                                           int input_width = 640,
                                           int input_height = 640);
        ~cvedix_yolo_rknn_face_detector_node();
    };
}

#endif // CVEDIX_WITH_RKNN

