#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include "rknn_yolov11.h"

namespace cvedix_nodes {
    // YOLOv11 detector optimized for Rockchip RKNN.
    // Now integrated with rknn_yolov11 detector implementation which supports
    // Split Head models and optimized DFL processing.
    class cvedix_rknn_yolov11_detector_node: public cvedix_primary_infer_node
    {
    private:
        std::shared_ptr<rknn_yolov11::rknn_yolov11_detector> detector;

        float score_threshold;
        float nms_threshold;
        int input_width;
        int input_height;
        int num_classes;

        // Temporary storage to bridge preprocess -> infer -> postprocess
        cv::Mat current_frame;
        rknn_yolov11::object_detect_result_list current_results;
        std::mutex process_mutex;

    protected:
        // Override preprocess to cache input frame
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;

        // Override infer to call detector->run_inference
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;

        // Override postprocess to convert results
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
