#pragma once

/**
 * @file cvedix_face_recognition_ort_node.h
 * @brief Face recognition node using ONNX Runtime (supports glintr100 and InsightFace models)
 * 
 * This node uses ONNX Runtime directly instead of OpenCV DNN, enabling use of
 * advanced InsightFace models that have compatibility issues with FaceRecognizerSF.
 * 
 * @note Requires ONNX Runtime library (libonnxruntime.so)
 */

#ifdef CVEDIX_WITH_ORT

#include "base/cvedix_secondary_infer_node.h"
#include "cvedix/utils/face/face_database_with_margin.h"
#include "cvedix/utils/face/face_recognition_utils.h"
#include <atomic>
#include <memory>
#include <onnxruntime_cxx_api.h>

namespace cvedix_nodes {

/**
 * @brief ONNX Runtime Face Recognizer helper class
 * 
 * Handles ONNX Runtime session management and embedding extraction.
 * Supports InsightFace models like glintr100, w600k_r50, etc.
 */
class OrtFaceRecognizer {
private:
    Ort::Env env_;
    Ort::Session session_{nullptr};
    Ort::AllocatorWithDefaultOptions allocator_;
    
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;
    int embedding_dim_ = 512;
    
    // Standard ArcFace reference points for 112x112
    const float arcface_dst_[5][2] = {
        {38.2946f, 51.6963f},
        {73.5318f, 51.5014f},
        {56.0252f, 71.7366f},
        {41.5493f, 92.3655f},
        {70.7299f, 92.2041f}
    };
    
public:
    /**
     * @brief Constructor - loads ONNX model
     * @param model_path Path to ONNX model file
     */
    OrtFaceRecognizer(const std::string& model_path);
    ~OrtFaceRecognizer();
    
    /**
     * @brief Get similarity transform matrix for face alignment
     * @param src Source 5-point landmarks [5][2]
     * @return 2x3 affine transformation matrix
     */
    cv::Mat getSimilarityTransformMatrix(float src[5][2]);
    
    /**
     * @brief Align and crop face using 5-point landmarks
     * @param src_img Source image
     * @param landmarks 5-point landmarks
     * @return Aligned 112x112 face image
     */
    cv::Mat alignCrop(const cv::Mat& src_img, float landmarks[5][2]);
    
    /**
     * @brief Extract embedding from aligned face
     * @param aligned_face Aligned 112x112 face image
     * @return L2-normalized embedding vector
     */
    std::vector<float> extractEmbedding(const cv::Mat& aligned_face);
    
    /**
     * @brief Get embedding dimension
     */
    int getEmbeddingDim() const { return embedding_dim_; }
};

/**
 * @brief Face recognition node using ONNX Runtime
 * 
 * Provides face recognition with optional accuracy improvements:
 * 
 * ## Techniques (configurable):
 * 1. **Temporal Voting** (default ON) - Stable results from video streams
 * 2. **TTA** (default OFF) - Test-time augmentation for higher accuracy
 * 3. **ID-Specific Threshold** (default OFF) - Per-person security settings
 * 
 * ## Key Differences from cvedix_face_recognition_node:
 * - Uses ONNX Runtime instead of OpenCV DNN
 * - Supports InsightFace models like glint360k_r100
 * - Preprocessing: (pixel - 127.5) / 127.5 → range [-1, 1]
 * 
 * @note Works with face detectors that provide 5-point landmarks
 */
class cvedix_face_recognition_ort_node : public cvedix_secondary_infer_node {
public:
    /**
     * @brief Constructor with default config (balanced mode)
     * @param node_name Name of the node
     * @param model_path Path to ONNX face recognition model
     * @param database_path Path to face database file
     * @param input_width Model input width (default 112)
     * @param input_height Model input height (default 112)
     * @param enable_alignment Enable face alignment (default true)
     */
    cvedix_face_recognition_ort_node(
        std::string node_name,
        std::string model_path,
        std::string database_path = "",
        int input_width = 112,
        int input_height = 112,
        bool enable_alignment = true
    );
    
    /**
     * @brief Constructor with custom recognition config
     * @param node_name Name of the node
     * @param model_path Path to ONNX face recognition model
     * @param database_path Path to face database file
     * @param config Recognition configuration
     * @param input_width Model input width (default 112)
     * @param input_height Model input height (default 112)
     */
    cvedix_face_recognition_ort_node(
        std::string node_name,
        std::string model_path,
        std::string database_path,
        const cvedix_face_utils::RecognitionConfig& config,
        int input_width = 112,
        int input_height = 112
    );
    
    virtual ~cvedix_face_recognition_ort_node();
    
    // ==================== Recognition API ====================
    
    /**
     * @brief Get recognition result for last processed face
     */
    cvedix_face_utils::MatchResult get_last_match() const { return last_match_; }
    
    /**
     * @brief Get stable result from voting buffer
     * @param track_id Face track ID
     */
    std::pair<std::string, float> get_stable_result(int track_id);
    
    // ==================== Database API ====================
    
    bool load_database(const std::string& path);
    cvedix_face_utils::EnhancedFaceDatabase& get_database() { return database_; }
    void print_database_stats();
    
    // ==================== Configuration API ====================
    
    void set_config(const cvedix_face_utils::RecognitionConfig& config);
    cvedix_face_utils::RecognitionConfig get_config() const { return config_; }
    void set_voting_enabled(bool enabled) { config_.voting_enabled = enabled; }
    void set_tta_enabled(bool enabled) { config_.tta.enabled = enabled; }
    void set_id_threshold_enabled(bool enabled) { config_.id_specific_threshold_enabled = enabled; }
    void set_personal_threshold(const std::string& name, float threshold);
    float get_personal_threshold(const std::string& name) const;
    
    // ==================== Direct Processing API ====================
    // For use outside of pipeline context
    
    /**
     * @brief Align face using 5-point landmarks
     * @param src_img Source image
     * @param landmarks 5-point landmarks [5][2]
     * @return Aligned 112x112 face image
     */
    cv::Mat alignFace(const cv::Mat& src_img, float landmarks[5][2]);
    
    /**
     * @brief Extract embedding from aligned face image
     * @param aligned_face Aligned 112x112 face image (BGR)
     * @return L2-normalized embedding vector
     */
    std::vector<float> extractEmbedding(const cv::Mat& aligned_face);
    
    /**
     * @brief Get embedding dimension
     */
    int getEmbeddingDim() const;
    
    // ==================== Legacy Configuration ====================
    
    bool enable_alignment;
    float similarity_threshold = 0.45f;  // Lower for glintr100
    float confidence_margin = 0.3f;
    
    // Statistics
    std::atomic<int> total_recognitions{0};
    
protected:
    virtual void prepare(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch,
        std::vector<cv::Mat>& mats_to_infer) override;
    
    virtual void preprocess(
        const std::vector<cv::Mat>& mats_to_infer,
        cv::Mat& blob_to_infer) override;
    
    virtual void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

private:
    // ORT Face Recognizer
    std::unique_ptr<OrtFaceRecognizer> ort_recognizer_;
    
    // Database
    cvedix_face_utils::EnhancedFaceDatabase database_;
    std::string database_path_;
    
    // Recognition state
    cvedix_face_utils::MatchResult last_match_;
    
    // Advanced techniques
    cvedix_face_utils::RecognitionConfig config_;
    std::unique_ptr<cvedix_face_utils::VotingBuffer> voting_buffer_;
    std::map<std::string, float> personal_thresholds_;
    
    // TTA state
    struct TTAInfo {
        int face_index;
        int augmentation_index;
    };
    std::vector<TTAInfo> current_tta_info_;
    
    // Aligned faces for postprocessing
    std::vector<cv::Mat> aligned_faces_;
    std::vector<int> face_indices_;
    
    // Embedding size
    int embedding_size_ = 512;
};

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_ORT
