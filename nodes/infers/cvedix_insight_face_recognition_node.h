#pragma once

#include "base/cvedix_secondary_infer_node.h"

namespace cvedix_nodes {
    /**
     * @brief InsightFace face recognition node (ONNX-based)
     * 
     * This node extracts face embeddings from detected faces using InsightFace models
     * (typically ArcFace) loaded directly from ONNX files. It works on cropped face regions
     * from face_targets and supports optional face alignment using 5-point landmarks.
     * 
     * Uses OpenCV DNN backend to load and run ONNX models, compatible with InsightFace
     * models from https://github.com/deepinsight/insightface
     */
    class cvedix_insight_face_recognition_node : public cvedix_secondary_infer_node {
    private:
        // Face alignment methods (ported from cvedix_sface_feature_encoder_node)
        cv::Mat getSimilarityTransformMatrix(float src[5][2]);
        void alignCrop(cv::Mat& src_img, float src_point[5][2], cv::Mat& aligned_img);
        
    protected:
        // Override prepare to handle face alignment if enabled
        virtual void prepare(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, 
                           std::vector<cv::Mat>& mats_to_infer) override;
        
        // Override preprocess to use InsightFace-specific normalization
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;
        
        // Override postprocess to extract embeddings and L2 normalize
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs,
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        /**
         * @brief Constructor
         * @param node_name Name of the node
         * @param model_path Path to ONNX model file (.onnx)
         * @param input_width Model input width (default: 112)
         * @param input_height Model input height (default: 112)
         * @param enable_alignment Whether to perform face alignment before recognition (default: true)
         */
        cvedix_insight_face_recognition_node(
            std::string node_name, 
            std::string model_path,
            int input_width = 112,
            int input_height = 112,
            bool enable_alignment = true
        );
        
        virtual ~cvedix_insight_face_recognition_node();
        
        // Whether face alignment is enabled
        bool enable_alignment;
        
        // Embedding dimension (auto-detected from model output)
        int embedding_size;
    };
}

