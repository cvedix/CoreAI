#pragma once

#ifdef CVEDIX_WITH_TRT
#include "base/cvedix_secondary_infer_node.h"
#include "cvedix/third_party/trt_insightface/models/insight_face_recognition.h"

namespace cvedix_nodes {
    /**
     * @brief TensorRT-based InsightFace face recognition node
     * 
     * This node extracts face embeddings from detected faces using InsightFace models
     * (typically ArcFace) accelerated with TensorRT. It works on cropped face regions
     * from face_targets and supports optional face alignment using 5-point landmarks.
     */
    class cvedix_trt_insight_face_recognition_node : public cvedix_secondary_infer_node {
    private:
        std::shared_ptr<trt_insightface::InsightFaceRecognition> recognizer;
        
        // Face alignment methods (ported from cvedix_sface_feature_encoder_node)
        cv::Mat getSimilarityTransformMatrix(float src[5][2]);
        void alignCrop(cv::Mat& src_img, float src_point[5][2], cv::Mat& aligned_img);
        
    protected:
        // Override prepare to handle face alignment if enabled
        virtual void prepare(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, 
                           std::vector<cv::Mat>& mats_to_infer) override;
        
        // Override run_infer_combinations to use TensorRT inference
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
        // Override postprocess (dummy implementation, not used)
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs,
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        /**
         * @brief Constructor
         * @param node_name Name of the node
         * @param model_path Path to TensorRT engine file (.engine)
         * @param input_width Model input width (default: 112)
         * @param input_height Model input height (default: 112)
         * @param enable_alignment Whether to perform face alignment before recognition (default: true)
         */
        cvedix_trt_insight_face_recognition_node(
            std::string node_name, 
            std::string model_path,
            int input_width = 112,
            int input_height = 112,
            bool enable_alignment = true
        );
        
        virtual ~cvedix_trt_insight_face_recognition_node();
        
        // Whether face alignment is enabled
        bool enable_alignment;
        
        /**
         * @brief Extract face embeddings directly from aligned face images
         * @param faces Vector of aligned face images (112x112 RGB)
         * @param embeddings Output vector of embedding vectors
         * 
         * This is a convenience method for direct embedding extraction without pipeline.
         * Useful for standalone face registration/recognition scenarios.
         */
        void extract_features(const std::vector<cv::Mat>& faces, std::vector<std::vector<float>>& embeddings) {
            if (recognizer) {
                recognizer->extract_features(faces, embeddings);
            } else {
                throw std::runtime_error("Recognizer not initialized");
            }
        }
    };
}

#endif  // CVEDIX_WITH_TRT

