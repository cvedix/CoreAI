/**
 * @file cvedix_yunet_face_detector_node.h
 * @brief YuNet face detector with 5-point landmarks
 * 
 * Fast, lightweight face detection using libfacedetection/YuNet.
 * Provides face bounding boxes and 5-point facial landmarks.
 * 
 * @see cvedix_face_recognition_node For face recognition
 */

#pragma once

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"

namespace cvedix_nodes {
    /**
     * @brief YuNet face detector
     * 
     * Lightweight face detection with 5-point landmarks.
     * @see https://github.com/ShiqiYu/libfacedetection
     */
    class cvedix_yunet_face_detector_node: public cvedix_primary_infer_node
    {

    private:
        // names of output layers in yunet
        const std::vector<std::string> out_names = {"loc", "conf", "iou"};
        float scoreThreshold = 0.7;
        float nmsThreshold = 0.5;
        int topK = 50;
        int inputW;
        int inputH;
        std::vector<cv::Rect2f> priors;
        void generatePriors();
    protected:
        // override infer and preprocess as yunet has a different logic
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) override;
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;

        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
    public:
        cvedix_yunet_face_detector_node(std::string node_name, std::string model_path, float score_threshold = 0.7, float nms_threshold = 0.5, int top_k = 50);
        ~cvedix_yunet_face_detector_node();
    };

}