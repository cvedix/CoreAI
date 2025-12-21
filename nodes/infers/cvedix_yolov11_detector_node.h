/**
 * @file cvedix_yolov11_detector_node.h
 * @brief YOLOv11 object detector using OpenCV DNN (ONNX)
 * 
 * Anchor-free detection with DFL (Distribution Focal Loss) processing.
 * 
 * @section yolov11_usage Usage
 * @code
 * auto detector = std::make_shared<cvedix_yolov11_detector_node>(
 *     "yolov11", "yolov11s.onnx", "coco.names",
 *     640, 640, 80
 * );
 * detector->attach_to({src_node});
 * @endcode
 * 
 * @see cvedix_yolo_detector_node For YOLOv3/v4/v5
 * @see cvedix_primary_infer_node Base class
 */

#pragma once

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"

namespace cvedix_nodes {

    /**
     * YOLOv11 detector using OpenCV DNN (ONNX model).
     * Supports anchor-free detection with DFL (Distribution Focal Loss) processing.
     * 
     * Model format: ONNX exported from YOLOv11 (Ultralytics)
     * Input: RGB image, normalized to [0, 1]
     * Output: Detections with bounding boxes and class probabilities
     */
    class cvedix_yolov11_detector_node: public cvedix_primary_infer_node
    {
    private:
        float score_threshold;
        float nms_threshold;
        int num_classes;
        
        // YOLOv11 postprocessing helpers
        void process_output(const cv::Mat& output,
                           const cv::Size& frame_size,
                           std::vector<int>& class_ids,
                           std::vector<float>& confidences,
                           std::vector<cv::Rect>& boxes);
        
    protected:
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs,
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        /**
         * Constructor for YOLOv11 ONNX detector
         * 
         * @param node_name         Name of the node
         * @param model_path        Path to ONNX model file
         * @param labels_path       Path to labels file (one class per line)
         * @param input_width       Model input width (default: 640)
         * @param input_height      Model input height (default: 640)
         * @param num_classes       Number of classes (default: 80 for COCO)
         * @param score_threshold   Score threshold for filtering detections (default: 0.25)
         * @param nms_threshold     NMS threshold (default: 0.45)
         * @param class_id_offset   Offset to add to class IDs (default: 0)
         */
        cvedix_yolov11_detector_node(std::string node_name,
                                    std::string model_path,
                                    std::string labels_path = "",
                                    int input_width = 640,
                                    int input_height = 640,
                                    int num_classes = 80,
                                    float score_threshold = 0.25f,
                                    float nms_threshold = 0.45f,
                                    int class_id_offset = 0);
        ~cvedix_yolov11_detector_node();
    };
}

