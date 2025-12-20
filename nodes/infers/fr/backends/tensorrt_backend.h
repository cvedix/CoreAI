/**
 * @file tensorrt_backend.h
 * @brief TensorRT backend for face recognition (requires CVEDIX_WITH_TRT)
 */

#pragma once

#ifdef CVEDIX_WITH_TRT

#include "../face_recognition_backend.h"
#include "cvedix/third_party/trt_insightface/models/insight_face_recognition.h"

namespace cvedix_nodes {

/**
 * @brief TensorRT backend implementation
 * 
 * Uses TensorRT for maximum inference performance on NVIDIA GPUs.
 * Typically ~5x faster than OpenCV DNN.
 */
class TensorRTBackend : public IFaceRecognitionBackend {
public:
    TensorRTBackend(const std::string& model_path, int input_width = 112, int input_height = 112)
        : model_path_(model_path), input_width_(input_width), input_height_(input_height) {
        
        try {
            // InsightFaceRecognition only takes engine_path, dimensions are fixed at 112x112
            recognizer_ = std::make_shared<trt_insightface::InsightFaceRecognition>(model_path);
            
            ready_ = true;
            CVEDIX_INFO("[TensorRTBackend] Loaded engine: " + model_path);
            
        } catch (const std::exception& e) {
            CVEDIX_ERROR("[TensorRTBackend] Exception: " + std::string(e.what()));
        }
    }
    
    std::vector<float> extractEmbedding(const cv::Mat& aligned_face) override {
        if (!ready_) {
            return std::vector<float>(embedding_dim_, 0.0f);
        }
        
        std::vector<cv::Mat> faces = {aligned_face};
        std::vector<std::vector<float>> embeddings;
        recognizer_->extract_features(faces, embeddings);
        
        if (embeddings.empty()) {
            return std::vector<float>(embedding_dim_, 0.0f);
        }
        
        // Normalize
        std::vector<float>& emb = embeddings[0];
        float norm = 0.0f;
        for (float v : emb) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 1e-6f) {
            for (float& v : emb) v /= norm;
        }
        
        embedding_dim_ = static_cast<int>(emb.size());
        return emb;
    }
    
    std::vector<std::vector<float>> extractEmbeddings(const std::vector<cv::Mat>& aligned_faces) override {
        if (!ready_ || aligned_faces.empty()) {
            return {};
        }
        
        std::vector<std::vector<float>> embeddings;
        recognizer_->extract_features(aligned_faces, embeddings);
        
        // Normalize all
        for (auto& emb : embeddings) {
            float norm = 0.0f;
            for (float v : emb) norm += v * v;
            norm = std::sqrt(norm);
            if (norm > 1e-6f) {
                for (float& v : emb) v /= norm;
            }
        }
        
        return embeddings;
    }
    
    int getEmbeddingDim() const override { return embedding_dim_; }
    std::string getBackendName() const override { return "TENSORRT"; }
    bool isReady() const override { return ready_; }
    
private:
    std::shared_ptr<trt_insightface::InsightFaceRecognition> recognizer_;
    std::string model_path_;
    int input_width_;
    int input_height_;
    mutable int embedding_dim_ = 512;
    bool ready_ = false;
};

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_TRT
