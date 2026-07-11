#include "cvedix_vlm_feature_node.h"

#ifdef CVEDIX_WITH_LLM

#include "cvedix/utils/logger/cvedix_logger.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace cvedix_nodes {

    namespace {
        constexpr int kEmbeddingDim = 128;
        constexpr int kMaxVlmInputEdge = 1024;

        bool read_bool_env(const char* key, bool default_value = false) {
            const char* value = std::getenv(key);
            if (!value || !*value) {
                return default_value;
            }

            std::string s(value);
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (s == "0" || s == "false" || s == "off" || s == "no") {
                return false;
            }
            if (s == "1" || s == "true" || s == "on" || s == "yes") {
                return true;
            }
            return default_value;
        }

        cv::Mat normalize_vlm_input_image(const cv::Mat& src) {
            if (src.empty()) {
                return {};
            }

            cv::Mat normalized;
            if (src.channels() == 3) {
                normalized = src;
            } else if (src.channels() == 4) {
                cv::cvtColor(src, normalized, cv::COLOR_BGRA2BGR);
            } else if (src.channels() == 1) {
                cv::cvtColor(src, normalized, cv::COLOR_GRAY2BGR);
            } else {
                cv::Mat tmp;
                src.convertTo(tmp, CV_8U);
                if (tmp.channels() == 1) {
                    cv::cvtColor(tmp, normalized, cv::COLOR_GRAY2BGR);
                } else if (tmp.channels() == 4) {
                    cv::cvtColor(tmp, normalized, cv::COLOR_BGRA2BGR);
                } else {
                    normalized = tmp;
                }
            }

            if (normalized.depth() != CV_8U) {
                cv::Mat tmp;
                normalized.convertTo(tmp, CV_8U);
                normalized = std::move(tmp);
            }

            if (normalized.channels() != 3) {
                return {};
            }

            const int max_edge = std::max(normalized.cols, normalized.rows);
            if (max_edge > kMaxVlmInputEdge) {
                const double scale = static_cast<double>(kMaxVlmInputEdge) / static_cast<double>(max_edge);
                cv::Mat resized;
                cv::resize(normalized, resized, cv::Size(), scale, scale, cv::INTER_AREA);
                normalized = std::move(resized);
            }

            if (!normalized.isContinuous()) {
                normalized = normalized.clone();
            }

            return normalized;
        }

        std::vector<float> histogram_embedding(const cv::Mat& bgr) {
            std::vector<float> emb(kEmbeddingDim, 0.0f);
            if (bgr.empty()) {
                return emb;
            }

            cv::Mat hsv;
            cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

            std::array<int, 8> h_hist{};
            std::array<int, 8> s_hist{};
            std::array<int, 8> v_hist{};

            for (int y = 0; y < hsv.rows; ++y) {
                const auto* row = hsv.ptr<cv::Vec3b>(y);
                for (int x = 0; x < hsv.cols; ++x) {
                    const auto& px = row[x];
                    h_hist[std::min(7, px[0] / 23)]++;
                    s_hist[std::min(7, px[1] / 32)]++;
                    v_hist[std::min(7, px[2] / 32)]++;
                }
            }

            const float total = static_cast<float>(hsv.rows * hsv.cols);
            for (int i = 0; i < 8; ++i) {
                emb[i] = h_hist[i] / std::max(1.0f, total);
                emb[8 + i] = s_hist[i] / std::max(1.0f, total);
                emb[16 + i] = v_hist[i] / std::max(1.0f, total);
            }

            cv::Mat gray;
            cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
            cv::Mat gx;
            cv::Mat gy;
            cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
            cv::Sobel(gray, gy, CV_32F, 0, 1, 3);
            cv::Mat mag;
            cv::magnitude(gx, gy, mag);

            std::array<int, 16> grad_hist{};
            const float grad_scale = 1024.0f;
            for (int y = 0; y < mag.rows; ++y) {
                const auto* row = mag.ptr<float>(y);
                for (int x = 0; x < mag.cols; ++x) {
                    int bin = static_cast<int>(row[x] / grad_scale * 16.0f);
                    bin = std::max(0, std::min(15, bin));
                    grad_hist[bin]++;
                }
            }
            for (int i = 0; i < 16; ++i) {
                emb[24 + i] = grad_hist[i] / std::max(1.0f, total);
            }

            const float ratio = static_cast<float>(bgr.cols) / std::max(1.0f, static_cast<float>(bgr.rows));
            emb[40] = ratio;
            emb[41] = static_cast<float>(bgr.cols);
            emb[42] = static_cast<float>(bgr.rows);

            for (int i = 43; i < kEmbeddingDim; ++i) {
                emb[i] = emb[i % 43] * 0.73f + static_cast<float>(i % 7) * 0.01f;
            }

            return emb;
        }

        std::string extract_json_substring(const std::string& input) {
            const auto begin = input.find('{');
            const auto end = input.rfind('}');
            if (begin == std::string::npos || end == std::string::npos || end <= begin) {
                return "";
            }
            return input.substr(begin, end - begin + 1);
        }
    }

    cvedix_vlm_feature_node::cvedix_vlm_feature_node(
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
        : cvedix_node(std::move(node_name)),
          cli_(api_base_url, api_key, backend_type),
          model_name_(std::move(model_name)),
          run_every_n_frames_(std::max(1, run_every_n_frames)),
          max_targets_per_frame_(std::max(1, max_targets_per_frame)),
          min_box_width_ratio_(std::max(0.0f, min_box_width_ratio)),
                    min_box_height_ratio_(std::max(0.0f, min_box_height_ratio)),
                    disable_remote_vlm_(
                            read_bool_env("CVEDIX_VLM_DISABLE_REMOTE", false) ||
                            read_bool_env("ASS_VLM_DISABLE_REMOTE", false)) {
        cli_.set_connection_timeout(std::max(1, request_timeout_sec));
        this->initialized();

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] VLM feature node initialized (model=%s, every_n=%d, max_targets=%d)",
            node_name.c_str(), model_name_.c_str(), run_every_n_frames_, max_targets_per_frame_));

        if (disable_remote_vlm_) {
            CVEDIX_WARN(cvedix_utils::string_format(
                "[%s] Remote VLM call disabled by env; node will use fallback embeddings only",
                node_name.c_str()));
        }
    }

    cvedix_vlm_feature_node::~cvedix_vlm_feature_node() {
        deinitialized();
    }

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_vlm_feature_node::handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_vlm_feature_node::handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (!meta || meta->frame.empty() || meta->targets.empty()) {
            return meta;
        }

        if (meta->frame_index >= 0 && (meta->frame_index % run_every_n_frames_) != 0) {
            return meta;
        }

        const int frame_w = meta->frame.cols;
        const int frame_h = meta->frame.rows;

        int processed = 0;
        for (auto& target_ptr : meta->targets) {
            if (!target_ptr) {
                continue;
            }
            if (processed >= max_targets_per_frame_) {
                break;
            }
            auto& target = *target_ptr;
            if (!should_process_target(target, frame_w, frame_h)) {
                continue;
            }

            const cv::Mat crop = crop_target_image(meta->frame, target);
            if (crop.empty()) {
                continue;
            }

            const std::string coarse = classify_coarse_category(target);
            auto result = extract_features_with_vlm(crop, coarse, target.primary_label);

            if (result.feature.empty()) {
                result = fallback_feature(crop, coarse);
            }
            normalize_l2(result.feature);

            target.embeddings = result.feature;
            upsert_vlm_secondary_entry(target, result);
            processed++;
        }

        return meta;
    }

    bool cvedix_vlm_feature_node::should_process_target(
        const cvedix_objects::cvedix_frame_target& target,
        int frame_w,
        int frame_h) const {
        if (target.width <= 1 || target.height <= 1 || frame_w <= 1 || frame_h <= 1) {
            return false;
        }
        const float w_ratio = static_cast<float>(target.width) / static_cast<float>(frame_w);
        const float h_ratio = static_cast<float>(target.height) / static_cast<float>(frame_h);
        return w_ratio >= min_box_width_ratio_ && h_ratio >= min_box_height_ratio_;
    }

    cv::Mat cvedix_vlm_feature_node::crop_target_image(
        const cv::Mat& frame,
        const cvedix_objects::cvedix_frame_target& target) const {
        const int x1 = std::max(0, target.x);
        const int y1 = std::max(0, target.y);
        const int x2 = std::min(frame.cols, target.x + target.width);
        const int y2 = std::min(frame.rows, target.y + target.height);
        if (x2 <= x1 || y2 <= y1) {
            return {};
        }
        return frame(cv::Rect(x1, y1, x2 - x1, y2 - y1)).clone();
    }

    std::string cvedix_vlm_feature_node::classify_coarse_category(
        const cvedix_objects::cvedix_frame_target& target) const {
        const std::string label = trim(target.primary_label);
        std::string l = label;
        std::transform(l.begin(), l.end(), l.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const std::vector<std::string> person_keys = {"person", "human", "nguoi"};
        for (const auto& k : person_keys) {
            if (l.find(k) != std::string::npos) return "person";
        }

        const std::vector<std::string> vehicle_keys = {
            "car", "bus", "truck", "motor", "bike", "vehicle", "xe", "van", "suv"};
        for (const auto& k : vehicle_keys) {
            if (l.find(k) != std::string::npos) return "vehicle";
        }

        const std::vector<std::string> animal_keys = {
            "dog", "cat", "bird", "horse", "cow", "sheep", "animal", "pet", "dong vat"};
        for (const auto& k : animal_keys) {
            if (l.find(k) != std::string::npos) return "animal";
        }

        return "object";
    }

    std::string cvedix_vlm_feature_node::build_prompt(
        const std::string& coarse_category,
        const std::string& label_hint) const {
        std::ostringstream oss;
        oss
            << "Analyze the cropped target image and output STRICT JSON only.\\n"
            << "Target coarse category: " << coarse_category << "\\n"
            << "Detection label hint: " << (label_hint.empty() ? "unknown" : label_hint) << "\\n"
            << "Return JSON schema exactly:\\n"
            << "{\"category\":\"person|vehicle|animal|object\","
            << "\"attributes\":\"short comma-separated attributes\"}\\n"
            << "Rules:\\n"
            << "1) no markdown, no explanation, JSON only.\\n"
            << "2) attributes should include visual traits: color, shape/type, texture, posture/state when visible.\\n"
            << "3) keep attributes short and literal, for example: blue shirt, beige shorts, walking.";
        return oss.str();
    }

    cvedix_vlm_feature_node::feature_result
    cvedix_vlm_feature_node::extract_features_with_vlm(
        const cv::Mat& crop,
        const std::string& coarse_category,
        const std::string& label_hint) {
        feature_result result;

        if (disable_remote_vlm_) {
            return result;
        }

        cv::Mat vlm_input = normalize_vlm_input_image(crop);
        if (vlm_input.empty()) {
            return result;
        }

        std::string content;
        {
            std::lock_guard<std::mutex> lock(llm_mutex_);
            content = cli_.simple_chat(model_name_, build_prompt(coarse_category, label_hint), {vlm_input}, {});
        }

        if (content.empty()) {
            return result;
        }

        if (!parse_llm_response(content, coarse_category, result)) {
            return {};
        }

        if (result.feature.empty()) {
            result.feature = histogram_embedding(vlm_input);
        }

        return result;
    }

    cvedix_vlm_feature_node::feature_result
    cvedix_vlm_feature_node::fallback_feature(
        const cv::Mat& crop,
        const std::string& coarse_category) const {
        feature_result result;
        result.category = coarse_category;
        result.attributes_summary = "fallback_visual_descriptor";
        result.feature = histogram_embedding(crop);
        result.from_fallback = true;
        return result;
    }

    bool cvedix_vlm_feature_node::parse_llm_response(
        const std::string& content,
        const std::string& coarse_category,
        feature_result& out) const {
        std::string payload = extract_json_substring(content);
        if (payload.empty()) {
            payload = content;
        }

        try {
            // Avoid new dependency include in this file by light parsing.
            // Expected fields are simple; parse via string search fallback.
            auto get_string_field = [&](const std::string& key) -> std::string {
                const std::string pat = "\"" + key + "\"";
                auto key_pos = payload.find(pat);
                if (key_pos == std::string::npos) return "";
                auto colon = payload.find(':', key_pos + pat.size());
                if (colon == std::string::npos) return "";
                auto q1 = payload.find('"', colon + 1);
                if (q1 == std::string::npos) return "";
                auto q2 = payload.find('"', q1 + 1);
                if (q2 == std::string::npos || q2 <= q1) return "";
                return trim(payload.substr(q1 + 1, q2 - q1 - 1));
            };

            const std::string category = get_string_field("category");
            const std::string attrs = get_string_field("attributes");

            std::vector<float> feat;

            auto feat_key = payload.find("\"feature\"");
            if (feat_key != std::string::npos) {
                auto lb = payload.find('[', feat_key);
                auto rb = payload.find(']', lb == std::string::npos ? feat_key : lb);
                if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                    std::string arr = payload.substr(lb + 1, rb - lb - 1);
                    feat.reserve(kEmbeddingDim);
                    std::stringstream ss(arr);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        token = trim(token);
                        if (token.empty()) continue;
                        try {
                            feat.push_back(std::stof(token));
                        } catch (...) {
                            // skip invalid token
                        }
                    }

                    if (!feat.empty()) {
                        if (feat.size() < kEmbeddingDim) {
                            const size_t old = feat.size();
                            for (size_t i = old; i < kEmbeddingDim; ++i) {
                                feat.push_back(feat[i % old] * 0.97f);
                            }
                        } else if (feat.size() > kEmbeddingDim) {
                            feat.resize(kEmbeddingDim);
                        }
                    }
                }
            }

            out.category = category.empty() ? coarse_category : category;
            out.attributes_summary = attrs.empty() ? "vlm_extracted" : attrs;
            out.feature = std::move(feat);
            out.from_fallback = false;
            return true;
        } catch (...) {
            return false;
        }
    }

    void cvedix_vlm_feature_node::normalize_l2(std::vector<float>& v) {
        if (v.empty()) return;
        double s = 0.0;
        for (float x : v) s += static_cast<double>(x) * static_cast<double>(x);
        const double n = std::sqrt(std::max(1e-12, s));
        for (auto& x : v) x = static_cast<float>(x / n);
    }

    std::string cvedix_vlm_feature_node::trim(std::string s) {
        auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        return s;
    }

    void cvedix_vlm_feature_node::upsert_vlm_secondary_entry(
        cvedix_objects::cvedix_frame_target& target,
        const feature_result& result) const {
        const std::string qlabel =
            "vlm:" + result.category + ":" + result.attributes_summary +
            (result.from_fallback ? ":fallback" : "");

        for (size_t i = 0; i < target.secondary_labels.size(); ++i) {
            if (target.secondary_labels[i].rfind("vlm:", 0) == 0) {
                target.secondary_labels[i] = qlabel;
                if (i < target.secondary_scores.size()) {
                    target.secondary_scores[i] = result.from_fallback ? 0.2f : 0.8f;
                }
                return;
            }
        }

        target.secondary_class_ids.push_back(-3001);
        target.secondary_scores.push_back(result.from_fallback ? 0.2f : 0.8f);
        target.secondary_labels.push_back(qlabel);
    }

}

#endif
