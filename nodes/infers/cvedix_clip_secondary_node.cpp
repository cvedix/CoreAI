/**
 * @file cvedix_clip_secondary_node.cpp
 * @brief CLIP per-target zero-shot classification implementation
 */

#include "cvedix_clip_secondary_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"

namespace cvedix_nodes {

cvedix_clip_secondary_node::cvedix_clip_secondary_node(
    const std::string& node_name,
    const std::string& image_encoder_path,
    const std::string& text_embeddings_path,
    const std::string& labels_path,
    int input_width,
    int input_height,
    int embed_dim,
    std::vector<int> p_class_ids_applied_to,
    bool store_embeddings,
    int top_k)
    : cvedix_secondary_infer_node(
          node_name, image_encoder_path, "", "",
          input_width, input_height, 1,
          p_class_ids_applied_to,
          0, 0, 10,             // min_width, min_height, crop_padding
          1.0f / 255.0f,        // scale
          cv::Scalar(0.48145466 * 255, 0.4578275 * 255, 0.40821073 * 255),  // mean
          cv::Scalar(0.26862954 * 255, 0.26130258 * 255, 0.27577711 * 255), // std
          true),                // swap_rb
      embed_dim(embed_dim),
      store_embeddings(store_embeddings),
      top_k(top_k) {

    if (!load_text_embeddings(text_embeddings_path)) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to load text embeddings from %s",
            node_name.c_str(), text_embeddings_path.c_str()));
    }

    if (!load_text_labels(labels_path)) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to load text labels from %s",
            node_name.c_str(), labels_path.c_str()));
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] CLIP secondary node: input=%dx%d, embed=%d, labels=%zu, store_embed=%d",
        node_name.c_str(), input_width, input_height, embed_dim,
        text_labels.size(), store_embeddings ? 1 : 0));

    this->initialized();
}

cvedix_clip_secondary_node::~cvedix_clip_secondary_node() {
    deinitialized();
}

bool cvedix_clip_secondary_node::load_text_embeddings(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    size_t total_floats = file_size / sizeof(float);
    if (total_floats % embed_dim != 0) return false;

    size_t num = total_floats / embed_dim;
    std::vector<float> data(total_floats);
    file.read(reinterpret_cast<char*>(data.data()), file_size);

    text_embeddings.clear();
    text_embeddings.resize(num);
    for (size_t i = 0; i < num; i++) {
        text_embeddings[i].assign(
            data.begin() + i * embed_dim,
            data.begin() + (i + 1) * embed_dim);
        l2_normalize(text_embeddings[i]);
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Loaded %zu text embeddings (%d-dim)",
        node_name.c_str(), num, embed_dim));
    return true;
}

bool cvedix_clip_secondary_node::load_text_labels(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    text_labels.clear();
    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) text_labels.push_back(line);
    }
    return true;
}

float cvedix_clip_secondary_node::cosine_similarity(
    const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    float dot = 0.0f;
    for (size_t i = 0; i < a.size(); i++) dot += a[i] * b[i];
    return dot;
}

void cvedix_clip_secondary_node::l2_normalize(std::vector<float>& v) {
    float norm = 0.0f;
    for (float val : v) norm += val * val;
    norm = std::sqrt(norm);
    if (norm > 1e-12f) {
        for (float& val : v) val /= norm;
    }
}

std::vector<float> cvedix_clip_secondary_node::softmax(
    const std::vector<float>& logits, float temperature) {
    std::vector<float> result(logits.size());
    float max_val = *std::max_element(logits.begin(), logits.end());
    float sum = 0.0f;
    for (size_t i = 0; i < logits.size(); i++) {
        result[i] = std::exp((logits[i] - max_val) * temperature);
        sum += result[i];
    }
    if (sum > 0.0f) {
        for (float& v : result) v /= sum;
    }
    return result;
}

void cvedix_clip_secondary_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (raw_outputs.empty() || text_embeddings.empty()) return;
    assert(frame_meta_with_batch.size() == 1);

    auto& output = raw_outputs[0];
    auto& frame_meta = frame_meta_with_batch[0];

    // Each row in output is one target's image embedding
    int count = output.rows;
    int output_dim = output.cols;

    if (output_dim != embed_dim) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Output dim (%d) != embed_dim (%d)",
            node_name.c_str(), output_dim, embed_dim));
        return;
    }

    int target_idx = 0;
    for (int i = 0; i < count; i++) {
        // Find the matching target (skip non-applicable ones)
        for (; target_idx < static_cast<int>(frame_meta->targets.size()); target_idx++) {
            auto& target = frame_meta->targets[target_idx];
            if (need_apply(target->primary_class_id, target->width, target->height)) {
                break;
            }
        }

        if (target_idx >= static_cast<int>(frame_meta->targets.size())) break;
        auto& target = frame_meta->targets[target_idx];

        // Extract image embedding from network output
        std::vector<float> image_embedding(embed_dim);
        const float* row_data = output.ptr<float>(i);
        std::copy(row_data, row_data + embed_dim, image_embedding.begin());
        l2_normalize(image_embedding);

        // Store embedding in target for downstream use (search, ReID)
        if (store_embeddings) {
            target->embeddings = image_embedding;
        }

        // Compute similarities with all text embeddings
        std::vector<float> similarities(text_embeddings.size());
        for (size_t t = 0; t < text_embeddings.size(); t++) {
            similarities[t] = cosine_similarity(image_embedding, text_embeddings[t]);
        }

        // Softmax to get probabilities
        std::vector<float> probs = softmax(similarities, 100.0f);

        // Sort indices by probability
        std::vector<int> indices(probs.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
            [&probs](int a, int b) { return probs[a] > probs[b]; });

        // Store top-K results in secondary fields
        int k = std::min(top_k, static_cast<int>(indices.size()));
        for (int j = 0; j < k; j++) {
            int idx = indices[j];
            std::string label = (idx < static_cast<int>(text_labels.size()))
                ? text_labels[idx]
                : "label_" + std::to_string(idx);
            target->secondary_class_ids.push_back(idx);
            target->secondary_labels.push_back(label);
            target->secondary_scores.push_back(probs[idx]);
        }

        target_idx++;
    }
}

bool cvedix_clip_secondary_node::reload_text_data(
    const std::string& embeddings_path,
    const std::string& labels_path) {
    std::lock_guard<std::mutex> lock(config_mutex);
    bool ok = true;
    if (!load_text_embeddings(embeddings_path)) ok = false;
    if (!load_text_labels(labels_path)) ok = false;
    return ok;
}

} // namespace cvedix_nodes
