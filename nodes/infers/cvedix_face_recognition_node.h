#pragma once

#include "base/cvedix_secondary_infer_node.h"
#include "cvedix/utils/face/face_database_with_margin.h"
#include "cvedix/utils/face/face_recognition_utils.h"
#include <atomic>
#include <memory>

namespace cvedix_nodes {

/**
 * @brief Face recognition node with advanced accuracy techniques
 * 
 * This node provides face recognition with optional accuracy improvements:
 * 
 * ## Techniques (configurable):
 * 1. **Temporal Voting** (default ON) - Stable results from video streams
 * 2. **TTA** (default OFF) - Test-time augmentation for higher accuracy
 * 3. **ID-Specific Threshold** (default OFF) - Per-person security settings
 * 
 * ## Base Features:
 * - Face alignment using 5-point landmarks
 * - InsightFace preprocessing: (pixel - 127.5) / 128.0, BGR→RGB
 * - L2 normalized embeddings with cosine similarity
 * - Margin-based confidence filtering
 * 
 * @note Works with face detectors that provide 5-point landmarks
 */
class cvedix_face_recognition_node : public cvedix_secondary_infer_node {
public:
    /**
     * @brief Constructor with default config (balanced mode)
     * @param node_name Name of the node
     * @param model_path Path to face recognition ONNX model
     * @param database_path Path to face database file
     * @param input_width Model input width (default 112)
     * @param input_height Model input height (default 112)
     * @param enable_alignment Enable face alignment (default true)
     */
    cvedix_face_recognition_node(
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
     * @param model_path Path to face recognition ONNX model
     * @param database_path Path to face database file
     * @param config Recognition configuration
     * @param input_width Model input width (default 112)
     * @param input_height Model input height (default 112)
     */
    cvedix_face_recognition_node(
        std::string node_name,
        std::string model_path,
        std::string database_path,
        const cvedix_face_utils::RecognitionConfig& config,
        int input_width = 112,
        int input_height = 112
    );
    
    virtual ~cvedix_face_recognition_node();
    
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
    
    /**
     * @brief Set recognition config
     */
    void set_config(const cvedix_face_utils::RecognitionConfig& config);
    
    /**
     * @brief Get current config
     */
    cvedix_face_utils::RecognitionConfig get_config() const { return config_; }
    
    /**
     * @brief Enable/disable temporal voting
     */
    void set_voting_enabled(bool enabled) { config_.voting_enabled = enabled; }
    
    /**
     * @brief Enable/disable TTA (reduces FPS ~3x)
     */
    void set_tta_enabled(bool enabled) { config_.tta.enabled = enabled; }
    
    /**
     * @brief Enable/disable ID-specific threshold
     */
    void set_id_threshold_enabled(bool enabled) { config_.id_specific_threshold_enabled = enabled; }
    
    /**
     * @brief Set personal threshold for a specific person
     * @param name Person name
     * @param threshold Personal similarity threshold
     */
    void set_personal_threshold(const std::string& name, float threshold);
    
    /**
     * @brief Get personal threshold for a person
     */
    float get_personal_threshold(const std::string& name) const;
    
    // ==================== Legacy Configuration ====================
    
    bool enable_alignment;
    float similarity_threshold = 0.7f;
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
    // Face alignment
    cv::Mat getSimilarityTransformMatrix(float src[5][2]);
    void alignCrop(cv::Mat& src_img, float src_point[5][2], cv::Mat& aligned_img);
    
    // Database
    cvedix_face_utils::EnhancedFaceDatabase database_;
    std::string database_path_;
    
    // Recognition state
    cvedix_face_utils::MatchResult last_match_;
    
    // Advanced techniques
    cvedix_face_utils::RecognitionConfig config_;
    std::unique_ptr<cvedix_face_utils::VotingBuffer> voting_buffer_;
    std::map<std::string, float> personal_thresholds_;  // ID-specific thresholds
    
    // TTA state
    struct TTAInfo {
        int face_index;
        int augmentation_index;  // 0=original, 1=flipped, etc.
    };
    std::vector<TTAInfo> current_tta_info_;
    
    // Embedding size
    int embedding_size_ = 512;
};

} // namespace cvedix_nodes
