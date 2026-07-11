/**
 * @file cvedix_vlm_feature_node.h
 * @brief Multi-model VLM-based object feature extraction node
 *
 * This node consumes detections from upstream detector/tracker nodes and uses
 * a configurable VLM model (via OpenAI-compatible or Ollama HTTP API) to
 * extract semantic features for each object crop.
 *
 * Output is written back to each target:
 * - target->embeddings: vector feature for downstream vector search/tracking
 * - target->secondary_labels: semantic summary prefixed with "vlm:"
 *
 * Typical pipeline:
 *   src -> detector -> vlm_feature -> broker/osd
 */

#pragma once

#ifdef CVEDIX_WITH_LLM

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/third_party/cpp_llmlib/llmlib.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace cvedix_nodes {

    class cvedix_vlm_feature_node : public cvedix_node {
    public:
        cvedix_vlm_feature_node(
            std::string node_name,
            std::string model_name = "qwen3-vl",
            std::string api_base_url = "http://127.0.0.1:11434",
            std::string api_key = "",
            llmlib::LLMBackendType backend_type = llmlib::LLMBackendType::Ollama,
            int run_every_n_frames = 6,
            int max_targets_per_frame = 6,
            float min_box_width_ratio = 0.03f,
            float min_box_height_ratio = 0.03f,
            int request_timeout_sec = 10);

        ~cvedix_vlm_feature_node() override;

    protected:
        std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

        std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
            std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

    private:
        struct feature_result {
            std::string category;
            std::string attributes_summary;
            std::vector<float> feature;
            bool from_fallback = false;
        };

        llmlib::LLMClient cli_;
        std::string model_name_;
        int run_every_n_frames_;
        int max_targets_per_frame_;
        float min_box_width_ratio_;
        float min_box_height_ratio_;
        bool disable_remote_vlm_ = false;
        std::mutex llm_mutex_;

        bool should_process_target(const cvedix_objects::cvedix_frame_target& target,
                                   int frame_w,
                                   int frame_h) const;

        cv::Mat crop_target_image(const cv::Mat& frame,
                                  const cvedix_objects::cvedix_frame_target& target) const;

        std::string classify_coarse_category(const cvedix_objects::cvedix_frame_target& target) const;

        std::string build_prompt(const std::string& coarse_category,
                                 const std::string& label_hint) const;

        feature_result extract_features_with_vlm(const cv::Mat& crop,
                                                 const std::string& coarse_category,
                                                 const std::string& label_hint);

        feature_result fallback_feature(const cv::Mat& crop,
                                        const std::string& coarse_category) const;

        bool parse_llm_response(const std::string& content,
                                const std::string& coarse_category,
                                feature_result& out) const;

        static void normalize_l2(std::vector<float>& v);
        static std::string trim(std::string s);

        void upsert_vlm_secondary_entry(cvedix_objects::cvedix_frame_target& target,
                                        const feature_result& result) const;
    };
}

#endif
