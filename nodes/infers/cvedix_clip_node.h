/**
 * @file cvedix_clip_node.h
 * @brief CLIP (Contrastive Language-Image Pretraining) zero-shot classification
 *
 * Performs zero-shot image classification by comparing frame/target embeddings
 * against pre-computed text embeddings using cosine similarity.
 *
 * @section clip_overview Overview
 * CLIP encodes images and text into a shared embedding space.
 * This node uses the image encoder to embed frames, then compares against
 * pre-computed text embeddings to find the best matching text description.
 *
 * @section clip_modes Two Modes
 * - **PRIMARY**: Classify whole frame → `frame_meta->description`
 * - **SECONDARY**: Classify each detected target → `target->secondary_labels`
 *
 * @section clip_models Supported Model Formats
 * - ONNX (via OpenCV DNN) — cross-platform
 * - TensorRT engine — NVIDIA GPU acceleration (compile with CVEDIX_WITH_TRT)
 *
 * @section clip_text_embeddings Text Embeddings
 * Text embeddings can be loaded from:
 * 1. Pre-computed binary file (`.bin` — float32 array)
 * 2. Pre-computed NumPy file (`.npy`) — requires conversion
 *
 * Generate text embeddings offline:
 * @code{.py}
 * import open_clip, torch, numpy as np
 * model, _, _ = open_clip.create_model_and_transforms('ViT-B-32', pretrained='laion2b_s34b_b79k')
 * tokenizer = open_clip.get_tokenizer('ViT-B-32')
 * labels = ["a car", "a truck", "a person", "a motorcycle"]
 * tokens = tokenizer(labels)
 * with torch.no_grad():
 *     text_features = model.encode_text(tokens)
 *     text_features /= text_features.norm(dim=-1, keepdim=True)
 * # Save as raw float32 binary: [N_labels × embed_dim]
 * text_features.cpu().numpy().astype(np.float32).tofile('clip_text_embeddings.bin')
 * @endcode
 *
 * @section clip_usage Usage
 * @code
 * auto clip = std::make_shared<cvedix_clip_node>(
 *     "clip",
 *     "clip_image_encoder.onnx",   // image encoder model
 *     "clip_text_embeddings.bin",   // pre-computed text embeddings
 *     "clip_labels.txt",            // text labels (one per line)
 *     224, 224,                     // CLIP input size
 *     512                           // embedding dimension
 * );
 * clip->attach_to({source_node});
 * @endcode
 *
 * @see cvedix_clip_secondary_node For per-target classification
 * @see cvedix_mllm_analyser_node For detailed scene description via LLM
 */

#pragma once

#include "base/cvedix_primary_infer_node.h"
#include <mutex>
#include <numeric>
#include <fstream>
#include <cmath>

namespace cvedix_nodes {

/**
 * @brief CLIP zero-shot classification node (full-frame primary)
 *
 * Encodes the entire frame using CLIP image encoder, then computes
 * cosine similarity with pre-computed text embeddings.
 *
 * Output stored in `frame_meta->description` as "label (score)".
 */
class cvedix_clip_node : public cvedix_primary_infer_node {
private:
    /// @brief Pre-computed text embeddings [N_labels × embed_dim]
    std::vector<std::vector<float>> text_embeddings;

    /// @brief Text labels corresponding to text_embeddings
    std::vector<std::string> text_labels;

    /// @brief Embedding dimension (e.g., 512 for ViT-B/32)
    int embed_dim;

    /// @brief CLIP-specific normalization
    cv::Scalar clip_mean = cv::Scalar(0.48145466 * 255, 0.4578275 * 255, 0.40821073 * 255);
    cv::Scalar clip_std = cv::Scalar(0.26862954 * 255, 0.26130258 * 255, 0.27577711 * 255);

    /// @brief Top-K results to include in description
    int top_k = 3;

    std::mutex config_mutex;

    /// @brief Load text embeddings from binary float32 file
    bool load_text_embeddings(const std::string& embeddings_path);

    /// @brief Load text labels from text file (one label per line)
    bool load_text_labels(const std::string& labels_path);

    /// @brief Compute cosine similarity between two vectors
    static float cosine_similarity(const std::vector<float>& a,
                                    const std::vector<float>& b);

    /// @brief L2 normalize a vector in-place
    static void l2_normalize(std::vector<float>& v);

    /// @brief Softmax over a vector
    static std::vector<float> softmax(const std::vector<float>& logits, float temperature = 100.0f);

protected:
    virtual void run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    virtual void postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

public:
    /**
     * @brief Constructor
     *
     * @param node_name Unique node identifier
     * @param image_encoder_path Path to CLIP image encoder (ONNX or TRT .engine)
     * @param text_embeddings_path Path to pre-computed text embeddings (.bin)
     * @param labels_path Path to text labels file (one per line)
     * @param input_width Model input width (default: 224 for ViT-B/32)
     * @param input_height Model input height (default: 224)
     * @param embed_dim Embedding dimension (default: 512)
     * @param top_k Top-K results to include (default: 3)
     */
    cvedix_clip_node(
        const std::string& node_name,
        const std::string& image_encoder_path,
        const std::string& text_embeddings_path,
        const std::string& labels_path,
        int input_width = 224,
        int input_height = 224,
        int embed_dim = 512,
        int top_k = 3);

    ~cvedix_clip_node();

    std::string to_string();

    /// @brief Get number of loaded text labels
    int get_num_labels() const { return text_labels.size(); }

    /// @brief Get text labels
    const std::vector<std::string>& get_text_labels() const { return text_labels; }

    /// @brief Set top-K results for description
    void set_top_k(int k) { top_k = k; }

    /// @brief Reload text embeddings and labels at runtime
    bool reload_text_data(const std::string& embeddings_path,
                          const std::string& labels_path);
};

} // namespace cvedix_nodes
