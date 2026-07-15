/**
 * @file cvedix_rapidmedia_vlm_feature_node.h
 * @brief RapidMedia-optimized VLM feature node built on cvedix_vlm_feature_node.
 *
 * This node keeps full compatibility with existing VLM extraction behavior while
 * adding a lightweight pre-filter/prioritization stage tailored for RapidMedia
 * event workloads (usually many small/low-value targets and limited VLM budget).
 */

#pragma once

#ifdef CVEDIX_WITH_LLM

#include "cvedix_vlm_feature_node.h"

#include <atomic>
#include <string>
#include <unordered_set>
#include <vector>

namespace cvedix_nodes {

    class cvedix_rapidmedia_vlm_feature_node : public cvedix_vlm_feature_node {
    public:
        cvedix_rapidmedia_vlm_feature_node(
            std::string node_name,
            std::string model_name = "qwen3-vl:2b",
            std::string api_base_url = "http://127.0.0.1:8080",
            std::string api_key = "",
            llmlib::LLMBackendType backend_type = llmlib::LLMBackendType::OpenAI,
            int run_every_n_frames = 4,
            int max_targets_per_frame = 4,
            float min_box_width_ratio = 0.02f,
            float min_box_height_ratio = 0.02f,
            int request_timeout_sec = 12);

        ~cvedix_rapidmedia_vlm_feature_node() override;

    protected:
        std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

    private:
        bool require_track_id_ = true;
        float min_target_area_ratio_ = 0.005f;
        int prefilter_max_targets_ = 12;
        std::unordered_set<std::string> allow_labels_;
        std::unordered_set<std::string> deny_labels_;
        std::unordered_set<std::string> priority_labels_;

        bool metrics_enabled_ = true;
        int metrics_report_interval_sec_ = 30;
        std::string metrics_log_prefix_ = "rapidmedia_vlm";

        std::atomic<unsigned long long> metric_frames_total_{0};
        std::atomic<unsigned long long> metric_frames_skipped_empty_{0};
        std::atomic<unsigned long long> metric_frames_skipped_invalid_{0};
        std::atomic<unsigned long long> metric_targets_input_total_{0};
        std::atomic<unsigned long long> metric_targets_kept_total_{0};
        std::atomic<unsigned long long> metric_targets_selected_total_{0};
        std::atomic<unsigned long long> metric_base_node_calls_{0};
        std::atomic<unsigned long long> metric_base_node_exceptions_{0};
        std::atomic<unsigned long long> metric_prefilter_time_us_total_{0};
        std::atomic<unsigned long long> metric_base_time_us_total_{0};
        std::atomic<long long> metric_last_report_epoch_sec_{0};

        static std::string normalize_label(const std::string& value);
        static std::unordered_set<std::string> parse_label_set(
            const char* raw,
            const std::unordered_set<std::string>& fallback);
        static bool read_bool_env(const char* key, bool fallback);
        static float read_float_env(const char* key, float fallback, float min_value, float max_value);
        static int read_int_env(const char* key, int fallback, int min_value, int max_value);

        bool should_keep_target(const cvedix_objects::cvedix_frame_target& target, int frame_w, int frame_h) const;
        double score_target_priority(const cvedix_objects::cvedix_frame_target& target, int frame_w, int frame_h) const;
        void report_metrics_if_due(bool force = false);
    };
}

#endif
