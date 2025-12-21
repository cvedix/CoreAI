/**
 * @file cvedix_yolo_detector_node.h
 * @brief YOLO object detector (v3/v4/v5) using OpenCV DNN
 * 
 * Primary detection node for YOLOv3/v4/v5 models.
 * 
 * @section yolo_usage Usage
 * @code
 * auto detector = std::make_shared<cvedix_yolo_detector_node>(
 *     "yolo", "yolov5s.onnx", "yolov5s.cfg", "coco.names",
 *     640, 640
 * );
 * detector->attach_to({src_node});
 * @endcode
 * 
 * @see cvedix_yolov11_detector_node For YOLOv11
 * @see cvedix_primary_infer_node Base class
 */

#pragma once

#include "base/cvedix_primary_infer_node.h"

namespace cvedix_nodes {
    /**
     * @brief YOLO object detector (v3/v4/v5)
     * 
     * Uses Darknet-style YOLO models via OpenCV DNN.
     * 
     * @see cvedix_primary_infer_node Base class
     */
    class cvedix_yolo_detector_node: public cvedix_primary_infer_node
    {

    private:
        float score_threshold;
        float confidence_threshold;
        float nms_threshold;
    protected:
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_yolo_detector_node(std::string node_name, 
                            std::string model_path, 
                            std::string model_config_path = "", 
                            std::string labels_path = "", 
                            int input_width = 416, 
                            int input_height = 416, 
                            int batch_size = 1,
                            int class_id_offset = 0,
                            float score_threshold = 0.5,
                            float confidence_threshold = 0.5,
                            float nms_threshold = 0.5,
                            float scale = 1 / 255.0,
                            cv::Scalar mean = cv::Scalar(0),
                            cv::Scalar std = cv::Scalar(1),
                            bool swap_rb = true);
        ~cvedix_yolo_detector_node();
    };
}