/**
 * @file cvedix_yolov11_plate_detector_node.h
 * @brief YOLOv11 License Plate detector using OpenCV DNN (ONNX)
 * 
 * Detects license plates in frames using YOLOv11 model.
 * 
 * @section yolov11_plate_usage Usage
 * @code
 * auto detector = std::make_shared<cvedix_yolov11_plate_detector_node>(
 *     "plate_detector", "license-plate-yolov11.onnx",
 *     640, 640
 * );
 * detector->attach_to({src_node});
 * @endcode
 * 
 * @see cvedix_yolov11_detector_node For general object detection
 * @see cvedix_primary_infer_node Base class
 */

#pragma once

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_target.h"

namespace cvedix_nodes {

    /**
     * YOLOv11 License Plate detector using OpenCV DNN (ONNX model).
     * 
     * Model format: ONNX exported from YOLOv11 fine-tuned for license plate detection
     * Input: RGB image, normalized to [0, 1]
     * Output: License plate bounding boxes
     */
    class cvedix_yolov11_plate_detector_node: public cvedix_primary_infer_node
    {
    private:
        float score_threshold;
        float nms_threshold;
        int num_classes;  // Typically 1 for plate-only detection
        
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
         * Constructor for YOLOv11 License Plate ONNX detector
         * 
         * @param node_name         Name of the node
         * @param model_path        Path to ONNX model file
         * @param input_width       Model input width (default: 640)
         * @param input_height      Model input height (default: 640)
         * @param num_classes       Number of classes (default: 1 for license plate)
         * @param score_threshold   Score threshold for filtering detections (default: 0.25)
         * @param nms_threshold     NMS threshold (default: 0.45)
         */
        cvedix_yolov11_plate_detector_node(std::string node_name,
                                    std::string model_path,
                                    int input_width = 640,
                                    int input_height = 640,
                                    int num_classes = 1,
                                    float score_threshold = 0.25f,
                                    float nms_threshold = 0.45f);
        ~cvedix_yolov11_plate_detector_node();
    };
}
