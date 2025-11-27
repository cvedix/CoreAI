
#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"
#include "cvedix/utils/rknn/cvedix_rknn_helper.h"
#ifdef CVEDIX_WITH_RGA
#include "cvedix/utils/rga/cvedix_rga_helper.h"  // RGA is optional
#else
// Forward declaration when RGA is not available
namespace cvedix_utils {
    class cvedix_rga_helper;
}
#endif

namespace cvedix_nodes {
    // Face detector optimized for Rockchip RKNN NPU
    // Based on YuNet architecture (libfacedetection)
    // https://github.com/ShiqiYu/libfacedetection
    // Supports 3 outputs: loc (location), conf (confidence), iou (intersection over union)
    // Automatically leverages NPU cores and optionally RGA for pre-processing acceleration
    class cvedix_rknn_face_detector_node: public cvedix_primary_infer_node
    {
    private:
        std::shared_ptr<cvedix_utils::cvedix_rknn_helper> rknn_helper;
        std::shared_ptr<cvedix_utils::cvedix_rga_helper> rga_helper;
        bool use_rga = false;
        
        float score_threshold;
        float nms_threshold;
        int top_k;
        int input_width;
        int input_height;
        
        // YuNet uses priors (anchor boxes) for detection
        std::vector<cv::Rect2f> priors;
        int current_input_w;
        int current_input_h;
        
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
        void generate_priors(int input_w, int input_h);
        
        // Parse YuNet outputs (loc, conf, iou) into face detections
        void parse_yunet_outputs(const cv::Mat& loc_mat,
                                const cv::Mat& conf_mat,
                                const cv::Mat& iou_mat,
                                const letterbox_params& lb_params,
                                const cv::Size& frame_size,
                                std::vector<cv::Rect>& boxes,
                                std::vector<float>& scores,
                                std::vector<std::vector<std::pair<int, int>>>& keypoints);
        
        // Parse multi-scale outputs (12 outputs: 3 scales x 4 outputs per scale)
        void parse_multiscale_outputs(const std::vector<cv::Mat>& raw_outputs,
                                     const letterbox_params& lb_params,
                                     const cv::Size& frame_size,
                                     std::vector<cv::Rect>& boxes,
                                     std::vector<float>& scores,
                                     std::vector<std::vector<std::pair<int, int>>>& keypoints);
        
    protected:
        // Override preprocess to use RGA if available
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;
        
        // Override infer to use RKNN API
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;
        
        // Override postprocess to parse YuNet output and create face targets
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, 
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        cvedix_rknn_face_detector_node(std::string node_name,
                                      std::string model_path,
                                      float score_threshold = 0.7f,
                                      float nms_threshold = 0.5f,
                                      int top_k = 50,
                                      int input_width = 320,
                                      int input_height = 320);
        ~cvedix_rknn_face_detector_node();
    };
}
