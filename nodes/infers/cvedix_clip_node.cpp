/**
 * @file cvedix_clip_node.cpp
 * @brief CLIP zero-shot classification implementation
 */

#include "cvedix_clip_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"

namespace cvedix_nodes {

cvedix_clip_node::cvedix_clip_node(
    const std::string& node_name,
    const std::string& image_encoder_path,
    const std::string& text_embeddings_path,
    const std::string& labels_path,
    int input_width,
    int input_height,
    int embed_dim,
    int top_k)
    : cvedix_primary_infer_node(
          node_name, image_encoder_path, "", "",
          input_width, input_height, 1,
          0,                // class_id_offset
          1.0f / 255.0f,    // scale
          cv::Scalar(0.48145466 * 255, 0.4578275 * 255, 0.40821073 * 255),  // mean
          cv::Scalar(0.26862954 * 255, 0.26130258 * 255, 0.27577711 * 255), // std
          true),            // swap_rb
      embed_dim(embed_dim),
      top_k(top_k) {

    // Load text embeddings
    if (!load_text_embeddings(text_embeddings_path)) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to load text embeddings from %s",
            node_name.c_str(), text_embeddings_path.c_str()));
    }

    // Load text labels
    if (!load_text_labels(labels_path)) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to load text labels from %s",
            node_name.c_str(), labels_path.c_str()));
    }

    // Validate consistency
    if (!text_embeddings.empty() && !text_labels.empty() &&
        text_embeddings.size() != text_labels.size()) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Mismatch: %zu embeddings vs %zu labels",
            node_name.c_str(), text_embeddings.size(), text_labels.size()));
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] CLIP node initialized: model=%s, input=%dx%d, embed_dim=%d, labels=%zu, top_k=%d",
        node_name.c_str(), image_encoder_path.c_str(),
        input_width, input_height, embed_dim,
        text_labels.size(), top_k));

    this->initialized();
}

cvedix_clip_node::~cvedix_clip_node() {
    deinitialized();
}

std::string cvedix_clip_node::to_string() {
    return cvedix_utils::string_format(
        "clip(input=%dx%d, embed_dim=%d, labels=%zu)",
        input_width, input_height, embed_dim, text_labels.size());
}

bool cvedix_clip_node::load_text_embeddings(const std::string& embeddings_path) {
    std::ifstream file(embeddings_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Calculate number of embeddings
    size_t total_floats = file_size / sizeof(float);
    if (total_floats % embed_dim != 0) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Text embeddings file size (%zu bytes) not divisible by embed_dim (%d)",
            node_name.c_str(), file_size, embed_dim));
        return false;
    }

    size_t num_embeddings = total_floats / embed_dim;

    // Read all floats
    std::vector<float> all_data(total_floats);
    file.read(reinterpret_cast<char*>(all_data.data()), file_size);

    // Split into individual embedding vectors
    text_embeddings.clear();
    text_embeddings.resize(num_embeddings);
    for (size_t i = 0; i < num_embeddings; i++) {
        text_embeddings[i].assign(
            all_data.begin() + i * embed_dim,
            all_data.begin() + (i + 1) * embed_dim);
        // Ensure normalized
        l2_normalize(text_embeddings[i]);
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Loaded %zu text embeddings (%d-dim) from %s",
        node_name.c_str(), num_embeddings, embed_dim, embeddings_path.c_str()));

    return true;
}

bool cvedix_clip_node::load_text_labels(const std::string& labels_path) {
    std::ifstream file(labels_path);
    if (!file.is_open()) {
        return false;
    }

    text_labels.clear();
    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            text_labels.push_back(line);
        }
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Loaded %zu text labels from %s",
        node_name.c_str(), text_labels.size(), labels_path.c_str()));

    return true;
}

float cvedix_clip_node::cosine_similarity(
    const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;

    float dot = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
    }
    // Vectors are already L2-normalized, so dot product = cosine similarity
    return dot;
}

void cvedix_clip_node::l2_normalize(std::vector<float>& v) {
    float norm = 0.0f;
    for (float val : v) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-12f) {
        for (float& val : v) {
            val /= norm;
        }
    }
}

std::vector<float> cvedix_clip_node::softmax(
    const std::vector<float>& logits, float temperature) {
    std::vector<float> result(logits.size());

    // Scale by temperature
    float max_val = *std::max_element(logits.begin(), logits.end());

    float sum = 0.0f;
    for (size_t i = 0; i < logits.size(); i++) {
        result[i] = std::exp((logits[i] - max_val) * temperature);
        sum += result[i];
    }

    if (sum > 0.0f) {
        for (float& v : result) {
            v /= sum;
        }
    }

    return result;
}

void cvedix_clip_node::run_infer_combinations(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (frame_meta_with_batch.empty() || text_embeddings.empty()) {
        return;
    }

    auto start_time = std::chrono::system_clock::now();

    for (auto& frame_meta : frame_meta_with_batch) {
        // Step 1: Preprocess — resize and normalize for CLIP
        cv::Mat resized;
        cv::resize(frame_meta->frame, resized, cv::Size(input_width, input_height));

        cv::Mat blob = cv::dnn::blobFromImage(
            resized, 1.0f / 255.0f,
            cv::Size(input_width, input_height),
            cv::Scalar(0.48145466 * 255, 0.4578275 * 255, 0.40821073 * 255),
            true, false);

        // Apply per-channel std normalization
        // CLIP std: [0.26862954, 0.26130258, 0.27577711]
        float* blob_data = blob.ptr<float>();
        int spatial = input_width * input_height;
        float std_vals[3] = {0.26862954f, 0.26130258f, 0.27577711f};
        for (int c = 0; c < 3; c++) {
            for (int j = 0; j < spatial; j++) {
                blob_data[c * spatial + j] /= std_vals[c];
            }
        }

        // Step 2: Forward pass through image encoder
        net.setInput(blob);
        cv::Mat output = net.forward();

        // Step 3: Extract image embedding
        std::vector<float> image_embedding(embed_dim);
        if (output.total() >= static_cast<size_t>(embed_dim)) {
            const float* output_data = output.ptr<float>();
            std::copy(output_data, output_data + embed_dim, image_embedding.begin());
        } else {
            CVEDIX_WARN(cvedix_utils::string_format(
                "[%s] Output size (%zu) < embed_dim (%d)",
                node_name.c_str(), output.total(), embed_dim));
            continue;
        }

        // Step 4: L2 normalize image embedding
        l2_normalize(image_embedding);

        // Step 5: Compute cosine similarity with all text embeddings
        std::vector<float> similarities(text_embeddings.size());
        for (size_t i = 0; i < text_embeddings.size(); i++) {
            similarities[i] = cosine_similarity(image_embedding, text_embeddings[i]);
        }

        // Step 6: Softmax to get probabilities
        std::vector<float> probs = softmax(similarities, 100.0f);

        // Step 7: Sort by probability (descending)
        std::vector<int> indices(probs.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(),
            [&probs](int a, int b) { return probs[a] > probs[b]; });

        // Step 8: Build description string with top-K results
        std::stringstream desc;
        int k = std::min(top_k, static_cast<int>(indices.size()));
        for (int i = 0; i < k; i++) {
            int idx = indices[i];
            if (i > 0) desc << " | ";
            std::string label = (idx < static_cast<int>(text_labels.size()))
                ? text_labels[idx]
                : "label_" + std::to_string(idx);
            desc << label << " ("
                 << std::fixed << std::setprecision(2)
                 << (probs[idx] * 100.0f) << "%)";
        }

        frame_meta->description = desc.str();

        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Frame %d: %s",
            node_name.c_str(), frame_meta->frame_index,
            frame_meta->description.c_str()));
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);
    infer_combinations_time_cost(frame_meta_with_batch.size(),
        0, 0, elapsed.count(), 0);
}

void cvedix_clip_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
    // Not used — all processing in run_infer_combinations
}

bool cvedix_clip_node::reload_text_data(
    const std::string& embeddings_path,
    const std::string& labels_path) {
    std::lock_guard<std::mutex> lock(config_mutex);
    bool ok = true;
    if (!load_text_embeddings(embeddings_path)) ok = false;
    if (!load_text_labels(labels_path)) ok = false;
    return ok;
}

} // namespace cvedix_nodes
