#pragma once

#include "../base/cvedix_secondary_infer_node.h"
#include "cvedix/utils/face/face_augmentation.h"
#include "cvedix/utils/face/face_database_with_margin.h"
#include <atomic>
#include <memory>

namespace cvedix_nodes {

/**
 * @brief Face registration node with augmentation support
 * 
 * This node provides face REGISTRATION capabilities with automatic data augmentation.
 * When registering a face, it generates multiple variants (with glasses, mask, hat)
 * to improve recognition robustness when the person wears accessories.
 * 
 * Key features:
 * - Face alignment using 5-point landmarks
 * - Automatic augmentation (glasses, mask, hat overlays)
 * - Stores embeddings to EnhancedFaceDatabase
 * - Outputs database file for use with cvedix_face_recognition_node
 * 
 * For recognition, use cvedix_face_recognition_node instead.
 * 
 * @note Requires CVEDIX_WITH_LICENSE
 * @note Requires face detector to provide face_targets with 5-point landmarks
 */
class cvedix_face_registration_node : public cvedix_secondary_infer_node {
public:
    /**
     * @brief Constructor
     * @param node_name Name of the node
     * @param model_path Path to face recognition ONNX model (InsightFace/ArcFace)
     * @param database_path Path to face database file (will be created/appended)
     * @param enable_augmentation Enable face augmentation during registration
     * @param glasses_template Path to glasses PNG template (optional)
     * @param mask_template Path to mask PNG template (optional)
     * @param hat_template Path to hat PNG template (optional)
     * @param input_width Model input width (default 112 for InsightFace)
     * @param input_height Model input height (default 112 for InsightFace)
     */
    cvedix_face_registration_node(
        std::string node_name,
        std::string model_path,
        std::string database_path,
        bool enable_augmentation = true,
        std::string glasses_template = "",
        std::string mask_template = "",
        std::string hat_template = "",
        int input_width = 112,
        int input_height = 112
    );
    
    virtual ~cvedix_face_registration_node();
    
    // ==================== Registration API ====================
    
    /**
     * @brief Register a face with the given name
     * 
     * Call this before processing a frame to set the name for the next detected face.
     * 
     * @param person_name Name of the person to register
     */
    void set_registration_name(const std::string& person_name);
    
    /**
     * @brief Get number of registered embeddings for last registration
     * This includes original + augmented variants
     */
    int get_last_registration_count() const { return last_registration_count_; }
    
    // ==================== Database API ====================
    
    /**
     * @brief Save database to file
     */
    bool save_database();
    
    /**
     * @brief Load existing database from file (for appending)
     */
    bool load_database();
    
    /**
     * @brief Clear all registered faces
     */
    void clear_database();
    
    /**
     * @brief Get database statistics
     */
    void print_database_stats();
    
    /**
     * @brief Get database reference
     */
    cvedix_face_utils::EnhancedFaceDatabase& get_database() { return database_; }
    
    // ==================== Configuration ====================
    
    bool enable_augmentation;       // Enable augmentation during registration
    
    // Statistics
    std::atomic<int> total_registrations{0};
    
protected:
    /**
     * @brief Prepare face images for inference
     * Override to handle augmentation
     */
    virtual void prepare(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch,
        std::vector<cv::Mat>& mats_to_infer) override;
    
    /**
     * @brief Preprocess images for InsightFace
     */
    virtual void preprocess(
        const std::vector<cv::Mat>& mats_to_infer,
        cv::Mat& blob_to_infer) override;
    
    /**
     * @brief Postprocess: store embeddings to database
     */
    virtual void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

private:
    // Face alignment
    cv::Mat getSimilarityTransformMatrix(float src[5][2]);
    void alignCrop(cv::Mat& src_img, float src_point[5][2], cv::Mat& aligned_img);
    
    // Components
    std::unique_ptr<cvedix_face_utils::FaceAugmenter> augmenter_;
    cvedix_face_utils::EnhancedFaceDatabase database_;
    
    // Database path
    std::string database_path_;
    
    // Registration state
    std::string pending_registration_name_;
    int last_registration_count_ = 0;
    
    // Track augmentation info for postprocess
    struct AugmentationInfo {
        int face_index;
        cvedix_face_utils::AugmentationType type;
    };
    std::vector<AugmentationInfo> current_augmentation_info_;
    
    // Embedding size
    int embedding_size_ = 512;
};

} // namespace cvedix_nodes
