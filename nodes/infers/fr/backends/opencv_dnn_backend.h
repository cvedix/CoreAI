/**
 * @file opencv_dnn_backend.h
 * @brief OpenCV DNN backend for face recognition (always available)
 */

#pragma once

#include "../face_recognition_backend.h"
#include <opencv2/dnn.hpp>

namespace cvedix_nodes {

/**
 * @brief OpenCV DNN backend implementation
 * 
 * Uses cv::dnn::readNetFromONNX for inference.
 * Supports CPU and CUDA (if CVEDIX_WITH_CUDA is defined).
 */
class OpenCVDnnBackend : public IFaceRecognitionBackend {
public:
    OpenCVDnnBackend(const std::string& model_path, int input_width = 112, int input_height = 112)
        : model_path_(model_path), input_width_(input_width), input_height_(input_height) {
        
        try {
            net_ = cv::dnn::readNetFromONNX(model_path);
            if (net_.empty()) {
                CVEDIX_ERROR("[OpenCVDnnBackend] Failed to load model: " + model_path);
                return;
            }
            
            #ifdef CVEDIX_WITH_CUDA
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            CVEDIX_INFO("[OpenCVDnnBackend] Using CUDA acceleration");
            #else
            net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            CVEDIX_INFO("[OpenCVDnnBackend] Using CPU");
            #endif
            
            // Detect embedding dimension
            detectEmbeddingDim();
            
            ready_ = true;
            CVEDIX_INFO("[OpenCVDnnBackend] Loaded model: " + model_path + " (emb_dim=" + std::to_string(embedding_dim_) + ")");
            
        } catch (const std::exception& e) {
            CVEDIX_ERROR("[OpenCVDnnBackend] Exception: " + std::string(e.what()));
        }
    }
    
    std::vector<float> extractEmbedding(const cv::Mat& aligned_face) override {
        if (!ready_) {
            return std::vector<float>(embedding_dim_, 0.0f);
        }
        
        cv::Mat blob;
        cv::dnn::blobFromImage(aligned_face, blob, 1.0f / 128.0f,
                               cv::Size(input_width_, input_height_),
                               cv::Scalar(127.5f, 127.5f, 127.5f),
                               true, false, CV_32F);
        
        net_.setInput(blob);
        cv::Mat output = net_.forward();
        
        // Extract and normalize embedding
        std::vector<float> embedding(embedding_dim_);
        const float* ptr = output.ptr<float>();
        
        float norm = 0.0f;
        for (int i = 0; i < embedding_dim_; i++) {
            embedding[i] = ptr[i];
            norm += embedding[i] * embedding[i];
        }
        norm = std::sqrt(norm);
        if (norm > 1e-6f) {
            for (float& v : embedding) v /= norm;
        }
        
        return embedding;
    }
    
    std::vector<std::vector<float>> extractEmbeddings(const std::vector<cv::Mat>& aligned_faces) override {
        std::vector<std::vector<float>> results;
        
        if (!ready_ || aligned_faces.empty()) {
            return results;
        }
        
        // Batch processing
        cv::Mat blob;
        cv::dnn::blobFromImages(aligned_faces, blob, 1.0f / 128.0f,
                                cv::Size(input_width_, input_height_),
                                cv::Scalar(127.5f, 127.5f, 127.5f),
                                true, false, CV_32F);
        
        net_.setInput(blob);
        cv::Mat output = net_.forward();
        
        int batch_size = output.size[0];
        int emb_dim = output.size[1];
        
        for (int i = 0; i < batch_size; i++) {
            std::vector<float> embedding(emb_dim);
            const float* ptr = output.ptr<float>(i);
            
            float norm = 0.0f;
            for (int j = 0; j < emb_dim; j++) {
                embedding[j] = ptr[j];
                norm += embedding[j] * embedding[j];
            }
            norm = std::sqrt(norm);
            if (norm > 1e-6f) {
                for (float& v : embedding) v /= norm;
            }
            
            results.push_back(embedding);
        }
        
        return results;
    }
    
    int getEmbeddingDim() const override { return embedding_dim_; }
    std::string getBackendName() const override { return "OPENCV_DNN"; }
    bool isReady() const override { return ready_; }
    
private:
    void detectEmbeddingDim() {
        try {
            cv::Mat dummy = cv::Mat::zeros(input_height_, input_width_, CV_8UC3);
            cv::Mat blob;
            cv::dnn::blobFromImage(dummy, blob, 1.0f / 128.0f,
                                   cv::Size(input_width_, input_height_),
                                   cv::Scalar(127.5f, 127.5f, 127.5f),
                                   true, false, CV_32F);
            net_.setInput(blob);
            cv::Mat output = net_.forward();
            if (output.dims >= 2) {
                embedding_dim_ = output.size[output.dims - 1];
            }
        } catch (...) {
            CVEDIX_WARN("[OpenCVDnnBackend] Could not detect embedding dim, using default 512");
        }
    }
    
    cv::dnn::Net net_;
    std::string model_path_;
    int input_width_;
    int input_height_;
    int embedding_dim_ = 512;
    bool ready_ = false;
};

} // namespace cvedix_nodes
