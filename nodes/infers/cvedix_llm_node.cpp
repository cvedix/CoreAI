/**
 * @file cvedix_llm_node.cpp
 * @brief Implementation of local LLM inference node
 */

#include "cvedix_llm_node.h"

#ifdef CVEDIX_WITH_LLM

#include <sstream>
#include <iomanip>
#include <chrono>

namespace cvedix_nodes {

// ─────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────

cvedix_llm_node::cvedix_llm_node(
    std::string node_name,
    std::string model_path,
    std::string prompt_template,
    int n_gpu_layers,
    int n_ctx,
    int max_tokens,
    float temperature,
    bool skip_on_busy)
    : cvedix_primary_infer_node(node_name, ""),  // no OpenCV DNN model
      prompt_template_(std::move(prompt_template)),
      skip_on_busy_(skip_on_busy)
{
    CVEDIX_INFO("[" + node_name + "] Initializing LLM local inference node");

    // Configure engine
    config_.model_path = model_path;
    config_.n_gpu_layers = n_gpu_layers;
    config_.n_ctx = n_ctx;
    config_.max_tokens = max_tokens;
    config_.temperature = temperature;
    config_.n_batch = 512;
    config_.n_threads = 4;
    config_.top_p = 0.9f;
    config_.top_k = 40;
    config_.repeat_penalty = 1.1f;

    // Create and load engine
    engine_ = std::make_unique<llm::LLMEngine>();

    auto load_start = std::chrono::steady_clock::now();
    bool loaded = engine_->load_model(config_);
    auto load_end = std::chrono::steady_clock::now();
    auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start).count();

    if (!loaded) {
        CVEDIX_ERROR("[" + node_name + "] Failed to load LLM model: " + model_path);
        throw std::runtime_error("[LLM] Failed to load model: " + model_path);
    }

    auto info = engine_->get_model_info();
    CVEDIX_INFO("[" + node_name + "] LLM model loaded in " + std::to_string(load_ms) + " ms");
    CVEDIX_INFO("[" + node_name + "] Model: " + info.name);
    CVEDIX_INFO("[" + node_name + "] VRAM: ~" + std::to_string(info.vram_usage_mb) + " MB");
    CVEDIX_INFO("[" + node_name + "] Context: " + std::to_string(n_ctx) + " tokens");

    this->initialized();
}

cvedix_llm_node::~cvedix_llm_node() {
    deinitialized();
    
    // Engine cleanup happens in unique_ptr destructor
    if (engine_ && engine_->is_loaded()) {
        CVEDIX_INFO("[" + node_name + "] Unloading LLM model");
        engine_->unload_model();
    }
}

// ─────────────────────────────────────────────
// Detection Formatting
// ─────────────────────────────────────────────

std::string cvedix_llm_node::format_detections(
    const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& frame_meta) 
{
    std::ostringstream ss;

    // Format standard object detection targets
    if (!frame_meta->targets.empty()) {
        ss << "Objects detected (" << frame_meta->targets.size() << "):\n";
        int idx = 1;
        for (const auto& target : frame_meta->targets) {
            ss << "  " << idx++ << ". ";
            
            // Class label
            if (!target->primary_label.empty()) {
                ss << target->primary_label;
            } else {
                ss << "class_" << target->primary_class_id;
            }

            // Confidence
            ss << " (confidence: " << std::fixed << std::setprecision(2) 
               << target->primary_score << ")";

            // Bounding box position
            ss << " at [" << target->x << ", " << target->y 
               << ", " << target->width << "x" << target->height << "]";

            // Tracking ID if available
            if (target->track_id >= 0) {
                ss << " track_id=" << target->track_id;
            }

            ss << "\n";
        }
    }

    // Format face targets if present
    if (!frame_meta->face_targets.empty()) {
        ss << "Faces detected (" << frame_meta->face_targets.size() << "):\n";
        int idx = 1;
        for (const auto& face : frame_meta->face_targets) {
            ss << "  " << idx++ << ". Face";
            ss << " (confidence: " << std::fixed << std::setprecision(2) 
               << face->score << ")";
            if (!face->identify.empty()) {
                ss << " identity: " << face->identify;
            }
            ss << "\n";
        }
    }

    // Format pose targets if present
    if (!frame_meta->pose_targets.empty()) {
        ss << "Poses detected (" << frame_meta->pose_targets.size() << "):\n";
    }

    // Include BA results if available
    if (!frame_meta->ba_results.empty()) {
        ss << "Behavior analysis events (" << frame_meta->ba_results.size() << "):\n";
        int idx = 1;
        for (const auto& ba : frame_meta->ba_results) {
            ss << "  " << idx++ << ". " << ba->ba_label 
               << " (type: " << cvedix_objects::cvedix_ba_result::ba_type_to_string(ba->type) << ")\n";
        }
    }

    // If nothing detected
    if (frame_meta->targets.empty() && 
        frame_meta->face_targets.empty() && 
        frame_meta->pose_targets.empty() &&
        frame_meta->ba_results.empty()) {
        ss << "No objects detected in this frame.\n";
    }

    return ss.str();
}

// ─────────────────────────────────────────────
// Prompt Building
// ─────────────────────────────────────────────

std::string cvedix_llm_node::build_prompt(
    const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& frame_meta) 
{
    std::string prompt = prompt_template_;

    // Replace {detections} placeholder
    std::string detections = format_detections(frame_meta);
    size_t pos = prompt.find("{detections}");
    if (pos != std::string::npos) {
        prompt.replace(pos, 12, detections);
    }

    // Replace {frame_index} placeholder
    pos = prompt.find("{frame_index}");
    if (pos != std::string::npos) {
        prompt.replace(pos, 13, std::to_string(frame_meta->frame_index));
    }

    // Replace {num_targets} placeholder
    pos = prompt.find("{num_targets}");
    if (pos != std::string::npos) {
        int total = frame_meta->targets.size() + 
                    frame_meta->face_targets.size() + 
                    frame_meta->pose_targets.size();
        prompt.replace(pos, 13, std::to_string(total));
    }

    return prompt;
}

// ─────────────────────────────────────────────
// Inference Pipeline
// ─────────────────────────────────────────────

void cvedix_llm_node::run_infer_combinations(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& 
    frame_meta_with_batch) 
{
    // LLM processes one frame at a time
    assert(frame_meta_with_batch.size() == 1);
    auto& frame_meta = frame_meta_with_batch[0];

    // Skip if busy and skip_on_busy is enabled
    if (skip_on_busy_ && busy_.load()) {
        CVEDIX_DEBUG("[" + node_name + "] Skipping frame " + 
                     std::to_string(frame_meta->frame_index) + " (LLM busy)");
        return;
    }

    // Serialize access to the engine
    std::lock_guard<std::mutex> lock(infer_mutex_);
    busy_ = true;

    auto start_time = std::chrono::steady_clock::now();

    // Build prompt from detection results
    std::string prompt = build_prompt(frame_meta);

    // Run inference
    auto result = engine_->generate(prompt);

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (result.success) {
        // Store result in frame metadata
        frame_meta->description = result.text;

        CVEDIX_DEBUG("[" + node_name + "] Frame " + 
                    std::to_string(frame_meta->frame_index) + 
                    " analyzed in " + std::to_string(elapsed_ms) + 
                    " ms, " + std::to_string(result.generated_tokens) + " tokens" +
                    ", " + std::to_string((int)result.tokens_per_second()) + " tok/s" +
                    ", output: " + std::to_string(result.text.length()) + " chars");
    } else {
        CVEDIX_ERROR("[" + node_name + "] Frame " + 
                    std::to_string(frame_meta->frame_index) + 
                    " LLM error: " + result.error);
    }

    busy_ = false;
}

void cvedix_llm_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& 
    frame_meta_with_batch) 
{
    // No-op: LLM handles everything in run_infer_combinations
}

// ─────────────────────────────────────────────
// Model Info
// ─────────────────────────────────────────────

llm::ModelInfo cvedix_llm_node::get_model_info() const {
    if (engine_ && engine_->is_loaded()) {
        return engine_->get_model_info();
    }
    return {};
}

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_LLM
