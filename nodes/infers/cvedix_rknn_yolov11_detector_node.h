#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include "cvedix/utils/rknn/cvedix_rknn_helper.h"
#ifdef CVEDIX_WITH_RGA
#include "cvedix/utils/rga/cvedix_rga_helper.h"
#else
namespace cvedix_utils {
    class cvedix_rga_helper;
}
#endif

namespace cvedix_nodes {
    // YOLOv11 detector optimized for Rockchip RKNN.
    // Reuses the generic RKNN + RGA pipeline from YOLOv8 detector and adapts
    // the parsing for YOLOv11-style outputs (cx, cy, w, h + scores).
    class cvedix_rknn_yolov11_detector_node: public cvedix_primary_infer_node
    {
    private:
        std::shared_ptr<cvedix_utils::cvedix_rknn_helper> rknn_helper;
        std::shared_ptr<cvedix_utils::cvedix_rga_helper> rga_helper;
        bool use_rga = false;

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

        // Parse YOLOv11 outputs into bounding boxes, scores and class ids.
        // The implementation is shared with YOLOv8 logic but tuned for the
        // typical YOLOv11 head layout (cx, cy, w, h, obj?, classes...).
        void parse_yolov11_output(const float* output_data,
                                  int output_size,
                                  const rknn_tensor_attr& output_attr,
                                  int parsed_num_classes,
                                  const letterbox_params& lb_params,
                                  const cv::Size& frame_size,
                                  std::vector<cv::Rect>& boxes,
                                  std::vector<float>& scores,
                                  std::vector<int>& class_ids);

    protected:
        // Override preprocess to use RGA if available (same strategy as YOLOv8).
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;

        // Override infer to use RKNN API.
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;

        // Override postprocess to parse YOLOv11 output and create frame targets.
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs,
                                 const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    public:
        cvedix_rknn_yolov11_detector_node(std::string node_name,
                                          std::string model_path,
                                          float score_threshold = 0.5f,
                                          float nms_threshold = 0.5f,
                                          int input_width = 640,
                                          int input_height = 640,
                                          int num_classes = 80,
                                          std::string labels_path = "",
                                          int class_id_offset = 0);
        ~cvedix_rknn_yolov11_detector_node();
    };
}


