/**
 * @file cvedix_face_detector_node.cpp
 * @brief Implementation of face detector using OpenCV FaceDetectorYN
 */

#include "cvedix_face_detector_node.h"

namespace cvedix_nodes {

    cvedix_face_detector_node::cvedix_face_detector_node(std::string node_name,
                                                          std::string model_path,
                                                          float score_threshold,
                                                          float nms_threshold,
                                                          int top_k)
        : cvedix_primary_infer_node(node_name, model_path),
          modelFilePath(model_path),
          scoreThreshold(score_threshold),
          nmsThreshold(nms_threshold),
          topK(top_k),
          cachedInputWidth(0),
          cachedInputHeight(0)
    {
        // Detector will be created lazily on first frame
        // to use actual frame dimensions
        this->initialized();
    }

    cvedix_face_detector_node::~cvedix_face_detector_node() {
        deinitialized();
    }

    void cvedix_face_detector_node::preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) {
        // FaceDetectorYN handles preprocessing internally
        // We just pass through the original image
        // Only support batch_size = 1 for FaceDetectorYN
        assert(mats_to_infer.size() == 1);
        blob_to_infer = mats_to_infer[0].clone();
    }

    void cvedix_face_detector_node::infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) {
        // blob_to_infer contains the original image (from preprocess)
        assert(!blob_to_infer.empty());
        
        int currentWidth = blob_to_infer.cols;
        int currentHeight = blob_to_infer.rows;
        
        // Create or recreate detector if frame dimensions changed
        if (!detector || currentWidth != cachedInputWidth || currentHeight != cachedInputHeight) {
            detector = cv::FaceDetectorYN::create(
                modelFilePath,
                "",  // config (empty for ONNX)
                cv::Size(currentWidth, currentHeight),
                scoreThreshold,
                nmsThreshold,
                topK
            );
            cachedInputWidth = currentWidth;
            cachedInputHeight = currentHeight;
        }
        
        assert(detector);
        
        // Run face detection
        cv::Mat faces;
        detector->detect(blob_to_infer, faces);
        
        raw_outputs.clear();
        raw_outputs.push_back(faces);
    }

    void cvedix_face_detector_node::postprocess(const std::vector<cv::Mat>& raw_outputs,
                                                 const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        assert(raw_outputs.size() == 1);
        assert(frame_meta_with_batch.size() == 1);
        
        const cv::Mat& faces = raw_outputs[0];
        auto& frame_meta = frame_meta_with_batch[0];
        
        // Process each detected face
        // FaceDetectorYN output format: each row contains 15 values
        // [x, y, w, h, re_x, re_y, le_x, le_y, nt_x, nt_y, rcm_x, rcm_y, lcm_x, lcm_y, score]
        // where:
        //   (x, y, w, h) - bounding box
        //   (re_x, re_y) - right eye
        //   (le_x, le_y) - left eye  
        //   (nt_x, nt_y) - nose tip
        //   (rcm_x, rcm_y) - right corner of mouth
        //   (lcm_x, lcm_y) - left corner of mouth
        //   score - confidence score
        
        for (int i = 0; i < faces.rows; i++) {
            // Extract bounding box
            int x = static_cast<int>(faces.at<float>(i, 0));
            int y = static_cast<int>(faces.at<float>(i, 1));
            int w = static_cast<int>(faces.at<float>(i, 2));
            int h = static_cast<int>(faces.at<float>(i, 3));
            
            // Clamp to frame boundaries
            x = std::max(x, 0);
            y = std::max(y, 0);
            w = std::min(w, frame_meta->frame.cols - x);
            h = std::min(h, frame_meta->frame.rows - y);
            
            // Skip invalid boxes
            if (w <= 0 || h <= 0) {
                continue;
            }
            
            // Extract 5 key points
            // Right eye
            auto kp_right_eye = std::make_pair(
                static_cast<int>(faces.at<float>(i, 4)),
                static_cast<int>(faces.at<float>(i, 5))
            );
            // Left eye
            auto kp_left_eye = std::make_pair(
                static_cast<int>(faces.at<float>(i, 6)),
                static_cast<int>(faces.at<float>(i, 7))
            );
            // Nose tip
            auto kp_nose = std::make_pair(
                static_cast<int>(faces.at<float>(i, 8)),
                static_cast<int>(faces.at<float>(i, 9))
            );
            // Right corner of mouth
            auto kp_mouth_right = std::make_pair(
                static_cast<int>(faces.at<float>(i, 10)),
                static_cast<int>(faces.at<float>(i, 11))
            );
            // Left corner of mouth
            auto kp_mouth_left = std::make_pair(
                static_cast<int>(faces.at<float>(i, 12)),
                static_cast<int>(faces.at<float>(i, 13))
            );
            
            // Get confidence score
            float score = faces.at<float>(i, 14);
            
            // Create face target with keypoints
            std::vector<std::pair<int, int>> keypoints = {
                kp_right_eye,
                kp_left_eye,
                kp_nose,
                kp_mouth_right,
                kp_mouth_left
            };
            
            auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                x, y, w, h, score, keypoints
            );
            
            frame_meta->face_targets.push_back(face_target);
        }
    }

    void cvedix_face_detector_node::setScoreThreshold(float threshold) {
        scoreThreshold = threshold;
        if (detector) {
            detector->setScoreThreshold(threshold);
        }
    }

    void cvedix_face_detector_node::setNMSThreshold(float threshold) {
        nmsThreshold = threshold;
        if (detector) {
            detector->setNMSThreshold(threshold);
        }
    }

    void cvedix_face_detector_node::setTopK(int k) {
        topK = k;
        if (detector) {
            detector->setTopK(k);
        }
    }

}
