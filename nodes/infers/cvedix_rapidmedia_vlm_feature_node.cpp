#include "cvedix_rapidmedia_vlm_feature_node.h"

#ifdef CVEDIX_WITH_LLM

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace cvedix_nodes {

    namespace {
        std::unordered_set<std::string> kDefaultAllowLabels = {
            "person", "human", "car", "bus", "truck", "motorbike", "motorcycle", "bicycle"
        };

        std::unordered_set<std::string> kDefaultPriorityLabels = {
            "person", "car", "motorbike", "motorcycle", "truck", "bus"
        };

        long long now_epoch_sec() {
            using namespace std::chrono;
            return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        }
    }

    cvedix_rapidmedia_vlm_feature_node::cvedix_rapidmedia_vlm_feature_node(
        std::string node_name,
        std::string model_name,
        std::string api_base_url,
        std::string api_key,
        llmlib::LLMBackendType backend_type,
        int run_every_n_frames,
        int max_targets_per_frame,
        float min_box_width_ratio,
        float min_box_height_ratio,
        int request_timeout_sec)
        : cvedix_vlm_feature_node(
            std::move(node_name),
            std::move(model_name),
            std::move(api_base_url),
            std::move(api_key),
            backend_type,
            run_every_n_frames,
            max_targets_per_frame,
            min_box_width_ratio,
            min_box_height_ratio,
            request_timeout_sec) {
        require_track_id_ = read_bool_env("ASS_RAPIDMEDIA_VLM_REQUIRE_TRACK", true);
        min_target_area_ratio_ = read_float_env("ASS_RAPIDMEDIA_VLM_MIN_AREA_RATIO", 0.005f, 0.0f, 1.0f);
        prefilter_max_targets_ = read_int_env("ASS_RAPIDMEDIA_VLM_PREFILTER_MAX", 12, 1, 128);
        allow_labels_ = parse_label_set(std::getenv("ASS_RAPIDMEDIA_VLM_ALLOW_LABELS"), kDefaultAllowLabels);
        deny_labels_ = parse_label_set(std::getenv("ASS_RAPIDMEDIA_VLM_DENY_LABELS"), {});
        priority_labels_ = parse_label_set(std::getenv("ASS_RAPIDMEDIA_VLM_PRIORITY_LABELS"), kDefaultPriorityLabels);

        metrics_enabled_ = read_bool_env("ASS_RAPIDMEDIA_VLM_METRICS_ENABLED", true);
        metrics_report_interval_sec_ = read_int_env("ASS_RAPIDMEDIA_VLM_METRICS_INTERVAL_SEC", 30, 1, 3600);
        const char* prefix = std::getenv("ASS_RAPIDMEDIA_VLM_METRICS_PREFIX");
        if (prefix && *prefix) {
            metrics_log_prefix_ = prefix;
        }

        CVEDIX_INFO(cvedix_utils::string_format(
            "[rapidmedia_vlm_metrics] enabled=%d interval_sec=%d prefix=%s",
            metrics_enabled_ ? 1 : 0,
            metrics_report_interval_sec_,
            metrics_log_prefix_.c_str()));
    }

    cvedix_rapidmedia_vlm_feature_node::~cvedix_rapidmedia_vlm_feature_node() {
        report_metrics_if_due(true);
    }

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_rapidmedia_vlm_feature_node::handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        metric_frames_total_.fetch_add(1, std::memory_order_relaxed);

        if (!meta || meta->frame.empty() || meta->targets.empty()) {
            metric_frames_skipped_empty_.fetch_add(1, std::memory_order_relaxed);
            report_metrics_if_due(false);
            return meta;
        }

        const int frame_w = meta->frame.cols;
        const int frame_h = meta->frame.rows;
        if (frame_w <= 1 || frame_h <= 1) {
            metric_frames_skipped_invalid_.fetch_add(1, std::memory_order_relaxed);
            report_metrics_if_due(false);
            return meta;
        }

        const auto prefilter_started = std::chrono::steady_clock::now();

        auto original_targets = meta->targets;
        metric_targets_input_total_.fetch_add(static_cast<unsigned long long>(original_targets.size()), std::memory_order_relaxed);

        struct candidate {
            double score;
            std::shared_ptr<cvedix_objects::cvedix_frame_target> target;
        };

        std::vector<candidate> candidates;
        candidates.reserve(original_targets.size());

        for (const auto& target_ptr : original_targets) {
            if (!target_ptr) {
                continue;
            }

            const auto& target = *target_ptr;
            if (!should_keep_target(target, frame_w, frame_h)) {
                continue;
            }

            candidates.push_back(candidate{
                score_target_priority(target, frame_w, frame_h),
                target_ptr,
            });
        }

        std::sort(candidates.begin(), candidates.end(), [](const candidate& a, const candidate& b) {
            if (a.score == b.score) {
                return a.target->primary_score > b.target->primary_score;
            }
            return a.score > b.score;
        });

        metric_targets_kept_total_.fetch_add(static_cast<unsigned long long>(candidates.size()), std::memory_order_relaxed);

        meta->targets.clear();
        meta->targets.reserve(static_cast<size_t>(std::min(prefilter_max_targets_, static_cast<int>(candidates.size()))));

        for (size_t i = 0; i < candidates.size(); ++i) {
            if (static_cast<int>(i) >= prefilter_max_targets_) {
                break;
            }
            meta->targets.push_back(candidates[i].target);
        }

        metric_targets_selected_total_.fetch_add(
            static_cast<unsigned long long>(meta->targets.size()),
            std::memory_order_relaxed);

        const auto prefilter_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - prefilter_started).count();
        metric_prefilter_time_us_total_.fetch_add(
            static_cast<unsigned long long>(std::max<long long>(0, prefilter_elapsed)),
            std::memory_order_relaxed);

        metric_base_node_calls_.fetch_add(1, std::memory_order_relaxed);
        const auto base_started = std::chrono::steady_clock::now();

        std::shared_ptr<cvedix_objects::cvedix_meta> result;
        try {
            result = cvedix_vlm_feature_node::handle_frame_meta(meta);
        } catch (...) {
            metric_base_node_exceptions_.fetch_add(1, std::memory_order_relaxed);
            const auto base_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - base_started).count();
            metric_base_time_us_total_.fetch_add(
                static_cast<unsigned long long>(std::max<long long>(0, base_elapsed)),
                std::memory_order_relaxed);
            meta->targets = std::move(original_targets);
            report_metrics_if_due(false);
            throw;
        }

        const auto base_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - base_started).count();
        metric_base_time_us_total_.fetch_add(
            static_cast<unsigned long long>(std::max<long long>(0, base_elapsed)),
            std::memory_order_relaxed);

        meta->targets = std::move(original_targets);
        report_metrics_if_due(false);
        return result;
    }

    std::string cvedix_rapidmedia_vlm_feature_node::normalize_label(const std::string& value) {
        std::string out;
        out.reserve(value.size());
        for (char ch : value) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (std::isalnum(c)) {
                out.push_back(static_cast<char>(std::tolower(c)));
            } else if (ch == '_' || ch == '-' || std::isspace(c)) {
                out.push_back('_');
            }
        }

        while (!out.empty() && out.front() == '_') {
            out.erase(out.begin());
        }
        while (!out.empty() && out.back() == '_') {
            out.pop_back();
        }
        return out;
    }

    std::unordered_set<std::string> cvedix_rapidmedia_vlm_feature_node::parse_label_set(
        const char* raw,
        const std::unordered_set<std::string>& fallback) {
        if (!raw || !*raw) {
            return fallback;
        }

        std::unordered_set<std::string> parsed;
        std::string token;
        for (const char* p = raw; ; ++p) {
            const char ch = *p;
            if (ch == ',' || ch == ';' || ch == '|' || ch == '\n' || ch == '\0') {
                const std::string normalized = normalize_label(token);
                if (!normalized.empty()) {
                    parsed.insert(normalized);
                }
                token.clear();
                if (ch == '\0') {
                    break;
                }
            } else {
                token.push_back(ch);
            }
        }

        return parsed.empty() ? fallback : parsed;
    }

    bool cvedix_rapidmedia_vlm_feature_node::read_bool_env(const char* key, bool fallback) {
        const char* raw = std::getenv(key);
        if (!raw || !*raw) {
            return fallback;
        }

        std::string normalized(raw);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes") {
            return true;
        }
        if (normalized == "0" || normalized == "false" || normalized == "off" || normalized == "no") {
            return false;
        }
        return fallback;
    }

    float cvedix_rapidmedia_vlm_feature_node::read_float_env(
        const char* key,
        float fallback,
        float min_value,
        float max_value) {
        const char* raw = std::getenv(key);
        if (!raw || !*raw) {
            return fallback;
        }

        char* end_ptr = nullptr;
        const float parsed = std::strtof(raw, &end_ptr);
        if (end_ptr == raw || !std::isfinite(parsed)) {
            return fallback;
        }
        return std::max(min_value, std::min(max_value, parsed));
    }

    int cvedix_rapidmedia_vlm_feature_node::read_int_env(
        const char* key,
        int fallback,
        int min_value,
        int max_value) {
        const char* raw = std::getenv(key);
        if (!raw || !*raw) {
            return fallback;
        }

        char* end_ptr = nullptr;
        const long parsed = std::strtol(raw, &end_ptr, 10);
        if (end_ptr == raw) {
            return fallback;
        }
        const long clamped = std::max(static_cast<long>(min_value), std::min(static_cast<long>(max_value), parsed));
        return static_cast<int>(clamped);
    }

    bool cvedix_rapidmedia_vlm_feature_node::should_keep_target(
        const cvedix_objects::cvedix_frame_target& target,
        int frame_w,
        int frame_h) const {
        if (target.width <= 1 || target.height <= 1) {
            return false;
        }

        if (require_track_id_ && target.track_id < 0) {
            return false;
        }

        const double target_area = static_cast<double>(target.width) * static_cast<double>(target.height);
        const double frame_area = static_cast<double>(frame_w) * static_cast<double>(frame_h);
        if (frame_area <= 1.0) {
            return false;
        }

        const double ratio = target_area / frame_area;
        if (ratio < static_cast<double>(min_target_area_ratio_)) {
            return false;
        }

        const std::string label = normalize_label(target.primary_label);
        if (!deny_labels_.empty() && deny_labels_.count(label) > 0) {
            return false;
        }
        if (!allow_labels_.empty() && allow_labels_.count(label) == 0) {
            return false;
        }

        return true;
    }

    double cvedix_rapidmedia_vlm_feature_node::score_target_priority(
        const cvedix_objects::cvedix_frame_target& target,
        int frame_w,
        int frame_h) const {
        const double frame_area = std::max(1.0, static_cast<double>(frame_w) * static_cast<double>(frame_h));
        const double area_ratio = (static_cast<double>(target.width) * static_cast<double>(target.height)) / frame_area;

        const std::string label = normalize_label(target.primary_label);
        const double priority_boost = priority_labels_.count(label) > 0 ? 2.0 : 0.0;
        const double tracked_boost = target.track_id >= 0 ? 0.5 : 0.0;
        const double detection_boost = std::max(0.0, static_cast<double>(target.primary_score));

        // Keep ordering simple and deterministic for real-time workloads.
        return area_ratio * 10.0 + priority_boost + tracked_boost + detection_boost * 0.2;
    }

    void cvedix_rapidmedia_vlm_feature_node::report_metrics_if_due(bool force) {
        if (!metrics_enabled_) {
            return;
        }

        const long long now_sec = now_epoch_sec();
        long long expected_last_sec = metric_last_report_epoch_sec_.load(std::memory_order_relaxed);
        if (!force && expected_last_sec > 0 &&
            (now_sec - expected_last_sec) < static_cast<long long>(metrics_report_interval_sec_)) {
            return;
        }
        if (!metric_last_report_epoch_sec_.compare_exchange_strong(
                expected_last_sec, now_sec, std::memory_order_relaxed, std::memory_order_relaxed)) {
            if (!force) {
                return;
            }
            metric_last_report_epoch_sec_.store(now_sec, std::memory_order_relaxed);
        }

        const unsigned long long frames_total = metric_frames_total_.load(std::memory_order_relaxed);
        const unsigned long long frames_empty = metric_frames_skipped_empty_.load(std::memory_order_relaxed);
        const unsigned long long frames_invalid = metric_frames_skipped_invalid_.load(std::memory_order_relaxed);
        const unsigned long long targets_input = metric_targets_input_total_.load(std::memory_order_relaxed);
        const unsigned long long targets_kept = metric_targets_kept_total_.load(std::memory_order_relaxed);
        const unsigned long long targets_selected = metric_targets_selected_total_.load(std::memory_order_relaxed);
        const unsigned long long base_calls = metric_base_node_calls_.load(std::memory_order_relaxed);
        const unsigned long long base_exceptions = metric_base_node_exceptions_.load(std::memory_order_relaxed);
        const unsigned long long prefilter_us_total = metric_prefilter_time_us_total_.load(std::memory_order_relaxed);
        const unsigned long long base_us_total = metric_base_time_us_total_.load(std::memory_order_relaxed);

        const double avg_prefilter_us = frames_total > 0
            ? static_cast<double>(prefilter_us_total) / static_cast<double>(frames_total)
            : 0.0;
        const double avg_base_us = base_calls > 0
            ? static_cast<double>(base_us_total) / static_cast<double>(base_calls)
            : 0.0;
        const double keep_ratio = targets_input > 0
            ? static_cast<double>(targets_kept) / static_cast<double>(targets_input)
            : 0.0;
        const double select_ratio = targets_input > 0
            ? static_cast<double>(targets_selected) / static_cast<double>(targets_input)
            : 0.0;

        std::ostringstream oss;
        oss << "[" << metrics_log_prefix_ << "]"
            << " frames_total=" << frames_total
            << " frames_skipped_empty=" << frames_empty
            << " frames_skipped_invalid=" << frames_invalid
            << " targets_input_total=" << targets_input
            << " targets_kept_total=" << targets_kept
            << " targets_selected_total=" << targets_selected
            << " keep_ratio=" << keep_ratio
            << " select_ratio=" << select_ratio
            << " base_node_calls=" << base_calls
            << " base_node_exceptions=" << base_exceptions
            << " prefilter_time_us_total=" << prefilter_us_total
            << " base_time_us_total=" << base_us_total
            << " prefilter_avg_us=" << avg_prefilter_us
            << " base_avg_us=" << avg_base_us;

        CVEDIX_INFO(oss.str());
    }
}

#endif
