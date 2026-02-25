/**
 * @file cvedix_clip_secondary_node.h
 * @brief CLIP zero-shot classification for detected targets (secondary/ROI-based)
 *
 * Classifies each detected target using CLIP image encoder + text embeddings.
 * Results stored in `target->secondary_labels`, `target->secondary_scores`,
 * and optionally `target->embeddings` for downstream search/ReID.
 *
 * @section clip_sec_pipeline Pipeline
 * @code
 * src → detector → tracker → clip_secondary → broker
 *                                  ↓
 *            target->secondary_labels = ["red car"]
 *            target->secondary_scores = [0.92]
 *            target->embeddings = [0.1, -0.3, ...]
 * @endcode
 *
 * @see cvedix_clip_node For full-frame classification
 * @see cvedix_classifier_node For standard secondary classification
 */

#pragma once

#include "base/cvedix_secondary_infer_node.h"
#include <mutex>
#include <numeric>
#include <fstream>
#include <cmath>

namespace cvedix_nodes {

/**
 * @brief CLIP zero-shot classification for detected targets
 *
 * Per-target version of CLIP. Crops each target from the frame,
 * runs CLIP image encoder, and classifies via cosine similarity
 * with pre-computed text embeddings.
 */
class cvedix_clip_secondary_node : public cvedix_secondary_infer_node {
private:
    /// @brief Pre-computed text embeddings [N_labels × embed_dim]
    std::vector<std::vector<float>> text_embeddings;

    /// @brief Text labels corresponding to embeddings
    std::vector<std::string> text_labels;

    /// @brief Embedding dimension
    int embed_dim;

    /// @brief Store image embeddings in target for ReID/search
    bool store_embeddings;

    /// @brief Top-K results per target
    int top_k;

    std::mutex config_mutex;

    bool load_text_embeddings(const std::string& path);
    bool load_text_labels(const std::string& path);
    static float cosine_similarity(const std::vector<float>& a,
                                    const std::vector<float>& b);
    static void l2_normalize(std::vector<float>& v);
    static std::vector<float> softmax(const std::vector<float>& logits,
                                       float temperature = 100.0f);

protected:
    virtual void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

public:
    /**
     * @brief Constructor
     *
     * @param node_name Unique node identifier
     * @param image_encoder_path Path to CLIP image encoder (ONNX)
     * @param text_embeddings_path Path to pre-computed text embeddings (.bin)
     * @param labels_path Path to text labels file
     * @param input_width CLIP input width (default: 224)
     * @param input_height CLIP input height (default: 224)
     * @param embed_dim Embedding dimension (default: 512)
     * @param p_class_ids_applied_to Primary class IDs to classify (empty = all)
     * @param store_embeddings Store image embeddings in target (default: true)
     * @param top_k Top-K results per target (default: 1)
     */
    cvedix_clip_secondary_node(
        const std::string& node_name,
        const std::string& image_encoder_path,
        const std::string& text_embeddings_path,
        const std::string& labels_path,
        int input_width = 224,
        int input_height = 224,
        int embed_dim = 512,
        std::vector<int> p_class_ids_applied_to = {},
        bool store_embeddings = true,
        int top_k = 1);

    ~cvedix_clip_secondary_node();

    /// @brief Reload text data at runtime
    bool reload_text_data(const std::string& embeddings_path,
                          const std::string& labels_path);
};

} // namespace cvedix_nodes
