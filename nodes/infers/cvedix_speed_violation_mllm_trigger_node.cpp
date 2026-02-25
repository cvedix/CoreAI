/**
 * @file cvedix_speed_violation_mllm_trigger_node.cpp
 * @brief Implementation of speed violation MLLM trigger node
 */

#ifdef CVEDIX_WITH_LLM

#include "cvedix_speed_violation_mllm_trigger_node.h"
#include "cvedix/third_party/cpp_llmlib/llmlib.hpp"
#include "cvedix/objects/ba/cvedix_ba_result.h"

#include <mutex>
#include <map>
#include <fstream>
#include <chrono>

namespace cvedix_nodes {

struct cvedix_speed_violation_mllm_trigger_node::Impl {
    llmlib::LLMClient llm_client;
    std::string llm_model;
    std::string analysis_prompt;
    std::string output_log_path;
    std::string m_node_name;
    std::mutex trigger_mutex;
    std::atomic<int> violation_count{0};
    std::map<int, int> last_analyzed_frame;
    int cooldown_frames;
};

cvedix_speed_violation_mllm_trigger_node::cvedix_speed_violation_mllm_trigger_node(
    const std::string& node_name,
    const std::string& model_name,
    const std::string& prompt,
    const std::string& api_url,
    const std::string& log_path,
    int cooldown_frames)
    : cvedix_node(node_name),
      pimpl(std::make_unique<Impl>()) {
    
    pimpl->llm_client = llmlib::LLMClient(api_url, "", llmlib::LLMBackendType::Ollama);
    pimpl->llm_model = model_name;
    pimpl->analysis_prompt = prompt;
    pimpl->output_log_path = log_path;
    pimpl->m_node_name = node_name;
    pimpl->cooldown_frames = cooldown_frames;

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Speed violation MLLM trigger initialized (model=%s, cooldown=%d frames)",
        node_name.c_str(), model_name.c_str(), cooldown_frames));
    this->initialized();
}

cvedix_speed_violation_mllm_trigger_node::~cvedix_speed_violation_mllm_trigger_node() {
    deinitialized();
}

int cvedix_speed_violation_mllm_trigger_node::get_violation_count() const {
    return pimpl->violation_count.load();
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_speed_violation_mllm_trigger_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    if (!meta || meta->frame.empty()) {
        return meta;
    }

    // Check for SPEED violation in BA results
    bool has_speed_violation = false;
    std::vector<int> violating_track_ids;

    for (const auto& ba_result : meta->ba_results) {
        if (!ba_result) continue;
        if (ba_result->ba_label.find("[VIOLATION]") != std::string::npos) {
            has_speed_violation = true;
            for (int tid : ba_result->involve_target_ids_in_frame) {
                violating_track_ids.push_back(tid);
            }
        }
    }

    if (!has_speed_violation) {
        return meta;  // No violation, pass through
    }

    std::lock_guard<std::mutex> lock(pimpl->trigger_mutex);

    for (int tid : violating_track_ids) {
        try {
            // Cooldown check
            if (pimpl->last_analyzed_frame.count(tid) > 0 &&
                (meta->frame_index - pimpl->last_analyzed_frame[tid]) < pimpl->cooldown_frames) {
                continue;
            }

            pimpl->last_analyzed_frame[tid] = meta->frame_index;
            pimpl->violation_count++;

            // Find and crop violating vehicle
            cv::Mat vehicle_crop;
            std::string speed_info;
            for (auto& target : meta->targets) {
                if (!target) continue;
                if (target->track_id == tid) {
                    // Safe crop with bounds checking
                    int x = std::max(0, target->x - 20);
                    int y = std::max(0, target->y - 20);
                    int w = std::min(target->width + 40, meta->frame.cols - x);
                    int h = std::min(target->height + 40, meta->frame.rows - y);

                    if (w <= 0 || h <= 0 || x < 0 || y < 0 ||
                        x + w > meta->frame.cols || y + h > meta->frame.rows) {
                        CVEDIX_WARN(cvedix_utils::string_format(
                            "[%s] Invalid crop rect for track %d: x=%d y=%d w=%d h=%d (frame: %dx%d)",
                            pimpl->m_node_name.c_str(), tid, x, y, w, h,
                            meta->frame.cols, meta->frame.rows));
                        continue;
                    }

                    vehicle_crop = meta->frame(cv::Rect(x, y, w, h)).clone();

                    for (const auto& label : target->secondary_labels) {
                        if (label.find("km/h") != std::string::npos) {
                            speed_info = label;
                            break;
                        }
                    }
                    break;
                }
            }

            if (vehicle_crop.empty()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Could not crop vehicle for track %d (not found in targets)",
                    pimpl->m_node_name.c_str(), tid));
                continue;
            }

            // Save crop image first (fast, always works)
            std::string crop_filename = "./output/violation_" +
                std::to_string(pimpl->violation_count) + "_track" +
                std::to_string(tid) + ".jpg";
            cv::imwrite(crop_filename, vehicle_crop);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Violation #%d detected | Track %d | %s | Saved: %s | Calling MLLM...",
                pimpl->m_node_name.c_str(), pimpl->violation_count.load(), tid,
                speed_info.c_str(), crop_filename.c_str()));

            // Call MLLM
            std::string full_prompt = pimpl->analysis_prompt;
            if (!speed_info.empty()) {
                full_prompt += "\nTốc độ đo được: " + speed_info;
            }

            auto start = std::chrono::system_clock::now();
            std::string result = pimpl->llm_client.simple_chat(
                pimpl->llm_model, full_prompt, {vehicle_crop}, {});
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - start);

            if (result.empty()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] MLLM returned empty result (is Ollama running at %s with model %s?)",
                    pimpl->m_node_name.c_str(), "localhost:11434", pimpl->llm_model.c_str()));
                continue;
            }

            // Store in frame description
            std::string desc = "[Violation #" + std::to_string(pimpl->violation_count) +
                "] Track " + std::to_string(tid) +
                " | " + speed_info +
                " | Analysis: " + result;
            meta->description = desc;

            // Log to file
            if (!pimpl->output_log_path.empty()) {
                std::ofstream log_file(pimpl->output_log_path, std::ios::app);
                if (log_file.is_open()) {
                    log_file << "=== Violation #" << pimpl->violation_count
                             << " ===" << std::endl;
                    log_file << "Frame: " << meta->frame_index << std::endl;
                    log_file << "Track ID: " << tid << std::endl;
                    log_file << "Speed: " << speed_info << std::endl;
                    log_file << "MLLM Analysis (" << elapsed.count() << "ms): "
                             << result << std::endl;
                    log_file << std::endl;
                }
            }

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Violation #%d | Track %d | %s | %s (%dms)",
                pimpl->m_node_name.c_str(), pimpl->violation_count.load(), tid,
                speed_info.c_str(),
                result.length() > 100 ? result.substr(0, 100).c_str() : result.c_str(),
                (int)elapsed.count()));

        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] MLLM analysis failed for track %d: %s",
                pimpl->m_node_name.c_str(), tid, e.what()));
        }
    }

    // Prune old cooldown entries
    for (auto it = pimpl->last_analyzed_frame.begin(); it != pimpl->last_analyzed_frame.end();) {
        if (meta->frame_index - it->second > pimpl->cooldown_frames * 3) {
            it = pimpl->last_analyzed_frame.erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

} // namespace cvedix_nodes

#endif
