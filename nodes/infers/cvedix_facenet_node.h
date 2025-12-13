#pragma once

#include "base/cvedix_secondary_infer_node.h"
#include "base/cvedix_primary_infer_node.h"

namespace cvedix_nodes {
    /**
     * @brief FaceNet face recognition node (ONNX-based)
     * 
     * This node implements FaceNet face recognition using InceptionResnetV1 architecture.
     * It extracts 512-dimensional face embeddings from detected faces.
     * 
     * Based on the facenet-pytorch implementation:
     * https://github.com/timesler/facenet-pytorch
     * 
     * Key features:
     * - InceptionResnetV1 architecture for face recognition
     * - Pretrained on VGGFace2 or CASIA-Webface datasets
     * - 512-dimensional face embeddings
     * - Optional face alignment using 5-point landmarks
     * - L2 normalization of embeddings
     * 
     * Model details:
     * - Input size: 160x160 (standard FaceNet input size)
     * - Input normalization: (pixel / 255.0 - 0.5) / 0.5 -> range [-1, 1]
     * - Color space: RGB
     * - Output: 512-dimensional embedding vector (L2 normalized)
     * 
     * Usage:
     * 1. Export PyTorch model to ONNX format using the provided conversion script
     * 2. Use with face detection node (e.g., MTCNN, YuNet)
     * 3. Works on cropped face regions from face_targets
     * 
     * @note This node requires ONNX model file exported from facenet-pytorch
     * @note For best results, use with MTCNN face detector and face alignment
     */
    class cvedix_facenet_node : public cvedix_secondary_infer_node {
    private:
        // Face alignment methods (same as InsightFace for consistency)
        cv::Mat getSimilarityTransformMatrix(float src[5][2]);
        void alignCrop(cv::Mat& src_img, float src_point[5][2], cv::Mat& aligned_img);
        
    protected:
        /**
         * @brief Prepare face images for inference
         * 
         * Handles face alignment if enabled and landmarks are available.
         * Otherwise performs simple crop and resize.
         */
        virtual void prepare(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, 
                           std::vector<cv::Mat>& mats_to_infer) override;
        
        /**
         * @brief Preprocess images for FaceNet
         * 
         * FaceNet preprocessing:
         * 1. Convert BGR to RGB
         * 2. Resize to 160x160
         * 3. Normalize: (pixel / 255.0 - 0.5) / 0.5 -> range [-1, 1]
         * 4. Create NCHW blob
         */
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) override;
        
        /**
         * @brief Postprocess FaceNet outputs
         * 
         * Extracts 512-dimensional embeddings and applies L2 normalization.
         * Stores embeddings in face_targets[i]->embeddings.
         */
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs,
                                const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Name of the node
         * @param model_path Path to ONNX model file (.onnx)
         *                   Use conversion script to export from facenet-pytorch:
         *                   python scripts/export_facenet_to_onnx.py
         * @param input_width Model input width (default: 160 for FaceNet)
         * @param input_height Model input height (default: 160 for FaceNet)
         * @param enable_alignment Whether to perform face alignment before recognition (default: true)
         *                        Requires 5-point facial landmarks from face detector
         * @param pretrained_dataset Dataset used for pretraining ("vggface2" or "casia-webface")
         *                          This is mainly for documentation/logging purposes
         */
        cvedix_facenet_node(
            std::string node_name, 
            std::string model_path,
            int input_width = 160,
            int input_height = 160,
            bool enable_alignment = true,
            std::string pretrained_dataset = "vggface2"
        );
        
        virtual ~cvedix_facenet_node();
        
        // Configuration
        bool enable_alignment;              // Whether to use face alignment
        std::string pretrained_dataset;     // Dataset used for pretraining
        int embedding_size;                 // Embedding dimension (512 for FaceNet)
        
        // Statistics
        int total_faces_processed = 0;      // Total number of faces processed
    };
    
    /**
     * @brief MTCNN face detector node for use with FaceNet
     * 
     * MTCNN (Multi-task Cascaded Convolutional Networks) is the recommended
     * face detector for FaceNet, as they were designed to work together.
     * 
     * MTCNN provides:
     * - Face bounding boxes
     * - 5 facial landmarks (eyes, nose, mouth corners)
     * - Face confidence scores
     * 
     * Architecture: 3-stage cascade (P-Net, R-Net, O-Net)
     * 
     * @note This is a primary infer node (detector)
     * @note Outputs face_targets with 5-point landmarks for alignment
     */
    class cvedix_mtcnn_face_detector_node : public cvedix_primary_infer_node {
    private:
        // MTCNN three-stage networks
        cv::dnn::Net pnet;  // Proposal Network (first stage)
        cv::dnn::Net rnet;  // Refine Network (second stage)
        cv::dnn::Net onet;  // Output Network (third stage)
        
        // MTCNN parameters
        float min_face_size;        // Minimum face size to detect (pixels)
        std::vector<float> thresholds;  // Thresholds for each stage [P, R, O]
        float nms_thresholds[3];    // NMS thresholds for each stage
        
        // Helper structures
        struct FaceBox {
            cv::Rect box;
            float score;
            std::vector<cv::Point2f> landmarks;  // 5 landmarks
        };
        
        // MTCNN implementation helpers
        void stage1_pnet(const cv::Mat& img, std::vector<FaceBox>& boxes);
        void stage2_rnet(const cv::Mat& img, std::vector<FaceBox>& boxes);
        void stage3_onet(const cv::Mat& img, std::vector<FaceBox>& boxes);
        void nms(std::vector<FaceBox>& boxes, float threshold);
        
    protected:
        virtual void run_infer_combinations(
            const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        
    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Name of the node
         * @param pnet_model Path to P-Net ONNX model
         * @param rnet_model Path to R-Net ONNX model  
         * @param onet_model Path to O-Net ONNX model
         * @param min_face_size Minimum face size to detect (default: 20)
         * @param thresholds Detection thresholds for [P, R, O] stages (default: [0.6, 0.7, 0.7])
         */
        cvedix_mtcnn_face_detector_node(
            std::string node_name,
            std::string pnet_model,
            std::string rnet_model,
            std::string onet_model,
            float min_face_size = 20.0f,
            std::vector<float> thresholds = {0.6f, 0.7f, 0.7f}
        );
        
        virtual ~cvedix_mtcnn_face_detector_node();
        
        // Configuration
        float score_threshold = 0.7f;  // Final detection confidence threshold
    };
}






