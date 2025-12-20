/**
 * @file face_recognition_backend.h
 * @brief Abstract backend interface for face recognition with auto-detection
 * 
 * Provides a unified interface for multiple face recognition backends:
 * - OpenCV DNN (always available)
 * - ONNX Runtime (if CVEDIX_WITH_ORT)
 * - TensorRT (if CVEDIX_WITH_TRT)
 */

#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include <string>
#include <memory>
#include "cvedix/utils/logger/cvedix_logger.h"

namespace cvedix_nodes {

/**
 * @brief Available face recognition backends
 */
enum class FaceRecognitionBackend {
    AUTO,           ///< Automatically select best available backend
    OPENCV_DNN,     ///< OpenCV DNN (always available, CPU/CUDA)
    ONNX_RUNTIME,   ///< ONNX Runtime (requires CVEDIX_WITH_ORT)
    TENSORRT        ///< TensorRT (requires CVEDIX_WITH_TRT)
};

/**
 * @brief Convert backend enum to string
 */
inline std::string backendToString(FaceRecognitionBackend backend) {
    switch (backend) {
        case FaceRecognitionBackend::AUTO: return "AUTO";
        case FaceRecognitionBackend::OPENCV_DNN: return "OPENCV_DNN";
        case FaceRecognitionBackend::ONNX_RUNTIME: return "ONNX_RUNTIME";
        case FaceRecognitionBackend::TENSORRT: return "TENSORRT";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Abstract interface for face recognition backends
 * 
 * Each backend implementation provides embedding extraction from aligned faces.
 */
class IFaceRecognitionBackend {
public:
    virtual ~IFaceRecognitionBackend() = default;
    
    /**
     * @brief Extract face embedding from aligned face image
     * @param aligned_face Aligned 112x112 RGB face image
     * @return Normalized embedding vector (typically 512-dim)
     */
    virtual std::vector<float> extractEmbedding(const cv::Mat& aligned_face) = 0;
    
    /**
     * @brief Extract embeddings from multiple faces (batch)
     * @param aligned_faces Vector of aligned face images
     * @return Vector of embedding vectors
     */
    virtual std::vector<std::vector<float>> extractEmbeddings(const std::vector<cv::Mat>& aligned_faces) {
        // Default: process one by one
        std::vector<std::vector<float>> results;
        for (const auto& face : aligned_faces) {
            results.push_back(extractEmbedding(face));
        }
        return results;
    }
    
    /**
     * @brief Get embedding dimension
     */
    virtual int getEmbeddingDim() const = 0;
    
    /**
     * @brief Get backend name for logging
     */
    virtual std::string getBackendName() const = 0;
    
    /**
     * @brief Check if backend is ready
     */
    virtual bool isReady() const = 0;
};

/**
 * @brief Detect best available backend at runtime
 * 
 * Priority: TensorRT > ONNX Runtime > OpenCV DNN
 */
inline FaceRecognitionBackend detectBestBackend() {
    #ifdef CVEDIX_WITH_TRT
    CVEDIX_DEBUG("[FaceRecognition] TensorRT backend available");
    return FaceRecognitionBackend::TENSORRT;
    #endif
    
    #ifdef CVEDIX_WITH_ORT
    CVEDIX_DEBUG("[FaceRecognition] ONNX Runtime backend available");
    return FaceRecognitionBackend::ONNX_RUNTIME;
    #endif
    
    CVEDIX_DEBUG("[FaceRecognition] Using OpenCV DNN backend (fallback)");
    return FaceRecognitionBackend::OPENCV_DNN;
}

/**
 * @brief Check if a specific backend is available
 */
inline bool isBackendAvailable(FaceRecognitionBackend backend) {
    switch (backend) {
        case FaceRecognitionBackend::OPENCV_DNN:
            return true;  // Always available
        case FaceRecognitionBackend::ONNX_RUNTIME:
            #ifdef CVEDIX_WITH_ORT
            return true;
            #else
            return false;
            #endif
        case FaceRecognitionBackend::TENSORRT:
            #ifdef CVEDIX_WITH_TRT
            return true;
            #else
            return false;
            #endif
        case FaceRecognitionBackend::AUTO:
            return true;
        default:
            return false;
    }
}

} // namespace cvedix_nodes
