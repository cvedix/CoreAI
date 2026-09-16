/**
 * @file cvedix_face_detector_node.h
 * @brief Face detector using OpenCV FaceDetectorYN (YuNet)
 * 
 * Face detection using OpenCV's high-level FaceDetectorYN API.
 * Provides face bounding boxes, 5-point facial landmarks, and confidence scores.
 * 
 * @section face_detector_usage Usage Example
 * @code
 * auto detector = std::make_shared<cvedix_face_detector_node>(
 *     "face_detector",
 *     "yunet.onnx",
 *     0.9f,              // score threshold (optional)
 *     0.3f,              // NMS threshold (optional)
 *     5000               // top_k (optional)
 * );
 * detector->attach_to({source_node});
 * @endcode
 * 
 * @see https://docs.opencv.org/4.x/df/d20/classcv_1_1FaceDetectorYN.html
 */

#pragma once

#include <opencv2/objdetect.hpp>
#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"

namespace cvedix_nodes {
    /**
     * @brief Face detector using OpenCV FaceDetectorYN
     * 
     * High-level face detection using OpenCV's built-in FaceDetectorYN.
     * Detects faces with 5-point landmarks (right eye, left eye, nose tip,
     * right mouth corner, left mouth corner).
     * 
     * Output format per face (15 values):
     * - [0-3]: x, y, width, height (bounding box)
     * - [4-5]: right eye (x, y)
     * - [6-7]: left eye (x, y)
     * - [8-9]: nose tip (x, y)
     * - [10-11]: right mouth corner (x, y)
     * - [12-13]: left mouth corner (x, y)
     * - [14]: confidence score
     * 
     * @see cvedix_primary_infer_node Base class
     */
    class cvedix_face_detector_node : public cvedix_primary_infer_node
    {

    private:
        /// @brief OpenCV FaceDetectorYN instance
        cv::Ptr<cv::FaceDetectorYN> detector;
        
        /// @brief Path to model file (stored locally for detector creation)
        std::string modelFilePath;
        
        /// @brief Score threshold for filtering detections
        float scoreThreshold;
        
        /// @brief NMS threshold for suppressing overlapping boxes
        float nmsThreshold;
        
        /// @brief Maximum number of detections to keep before NMS
        int topK;
        
        /// @brief Cached input width for dynamic size adjustment
        int cachedInputWidth;
        
        /// @brief Cached input height for dynamic size adjustment
        int cachedInputHeight;

    protected:
        /**
         * @brief Override infer to use FaceDetectorYN instead of cv::dnn::Net
         * @param blob_to_infer Input blob (not used, detection runs on original image)
         * @param raw_outputs Output containing detected faces matrix
         */
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;
        
        /**
         * @brief Override preprocess - FaceDetectorYN handles preprocessing internally
         * @param mats_to_infer Input images
         * @param blob_to_infer Output blob (stores original image for detection)
         */
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;
        
        /**
         * @brief Parse detected faces and create face targets
         * @param raw_outputs Detection results from FaceDetectorYN
         * @param frame_meta_with_batch Frame metas to update with face targets
         */
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, 
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param model_path Path to YuNet ONNX model file
         * @param score_threshold Minimum confidence score (default: 0.9)
         * @param nms_threshold NMS IoU threshold (default: 0.3)
         * @param top_k Max detections before NMS (default: 5000)
         */
        cvedix_face_detector_node(std::string node_name, 
                                  std::string model_path,
                                  float score_threshold = 0.9f,
                                  float nms_threshold = 0.3f,
                                  int top_k = 5000);
        
        ~cvedix_face_detector_node();
        
        /**
         * @brief Set score threshold
         * @param threshold New score threshold (0.0 - 1.0)
         */
        void setScoreThreshold(float threshold);
        
        /**
         * @brief Set NMS threshold
         * @param threshold New NMS threshold (0.0 - 1.0)
         */
        void setNMSThreshold(float threshold);
        
        /**
         * @brief Set top-k value
         * @param k New top-k value
         */
        void setTopK(int k);
    };

}
