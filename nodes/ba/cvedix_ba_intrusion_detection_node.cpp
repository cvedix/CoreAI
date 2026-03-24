#include "cvedix_ba_intrusion_detection_node.h"

namespace cvedix_nodes {

// ─── Constructors ────────────────────────────────────────────────

cvedix_ba_intrusion_detection_node::cvedix_ba_intrusion_detection_node(
    std::string node_name,
    std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
    std::map<int, std::vector<intrusion_config>> configs,
    bool need_record_image, bool need_record_video,
    bool include_target_crops)
    : cvedix_node(node_name), all_areas(areas),
      need_record_image(need_record_image),
      need_record_video(need_record_video),
      include_target_crops(include_target_crops) {

    // Copy provided configs; fill defaults where missing
    for (const auto &ch : all_areas) {
        int cid = ch.first;
        size_t n = ch.second.size();
        if (configs.count(cid) > 0) {
            all_configs[cid] = configs.at(cid);
            all_configs[cid].resize(n); // pad with defaults if short
        } else {
            all_configs[cid].resize(n);
        }
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] %s", node_name.c_str(), to_string().c_str()));
    this->initialized();
}

cvedix_ba_intrusion_detection_node::cvedix_ba_intrusion_detection_node(
    std::string node_name,
    std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
    bool need_record_image, bool need_record_video,
    bool include_target_crops)
    : cvedix_node(node_name), all_areas(areas),
      need_record_image(need_record_image),
      need_record_video(need_record_video),
      include_target_crops(include_target_crops) {

    for (const auto &ch : all_areas) {
        all_configs[ch.first].resize(ch.second.size());
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] %s", node_name.c_str(), to_string().c_str()));
    this->initialized();
}

cvedix_ba_intrusion_detection_node::~cvedix_ba_intrusion_detection_node() {
    deinitialized();
}

// ─── Helpers ─────────────────────────────────────────────────────

std::string cvedix_ba_intrusion_detection_node::to_string() {
    std::lock_guard<std::mutex> lock(areas_mutex);
    std::stringstream ss;
    for (const auto &ch : all_areas) {
        ss << "[channel" << ch.first << ": ";
        for (size_t i = 0; i < ch.second.size(); ++i) {
            ss << "zone" << i << "(";
            if (i < all_configs[ch.first].size() && !all_configs[ch.first][i].name.empty())
                ss << all_configs[ch.first][i].name << ", ";
            ss << ch.second[i].size() << "pts) ";
        }
        ss << "]";
    }
    return ss.str();
}

bool cvedix_ba_intrusion_detection_node::is_inside_polygon(
    const cvedix_objects::cvedix_point &p,
    const std::vector<cvedix_objects::cvedix_point> &polygon) {
    bool inside = false;
    size_t n = polygon.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        double xi = polygon[i].x, yi = polygon[i].y;
        double xj = polygon[j].x, yj = polygon[j].y;
        bool intersect = ((yi > p.y) != (yj > p.y)) &&
            (p.x < (xj - xi) * (p.y - yi) / (yj - yi + 1e-12) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

std::set<int> cvedix_ba_intrusion_detection_node::get_areas_containing_point(
    const cvedix_objects::cvedix_rect &bbox,
    const std::vector<std::vector<cvedix_objects::cvedix_point>> &areas,
    const std::vector<intrusion_config> &configs) {
    std::set<int> result;
    for (size_t i = 0; i < areas.size(); ++i) {
        auto pt = (i < configs.size())
            ? bbox.track_point(configs[i].anchor_point) : bbox.track_point();
        if (is_inside_polygon(pt, areas[i]))
            result.insert(i);
    }
    return result;
}

double cvedix_ba_intrusion_detection_node::now_ms() {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

// ─── Main Logic ──────────────────────────────────────────────────

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_intrusion_detection_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(areas_mutex);

    if (all_areas.count(meta->channel_index) == 0 || all_areas[meta->channel_index].empty())
        return meta;

    const auto &channel_areas = all_areas[meta->channel_index];
    auto &configs = all_configs[meta->channel_index];
    auto &prev_status = previous_area_status[meta->channel_index];

    double current_ms = now_ms();

    for (auto &target : meta->targets) {
        auto len = target->tracks.size();
        if (len <= 1 || target->track_id < 0) continue;

        auto current_bbox = target->tracks[len - 1];
        auto current_areas = get_areas_containing_point(current_bbox, channel_areas, configs);
        auto &prev_areas = prev_status[target->track_id];

        // ── ENTER: in current but not in previous ──
        for (int ai : current_areas) {
            if (prev_areas.find(ai) != prev_areas.end()) continue; // already inside

            const auto &config = (ai < (int)configs.size()) ? configs[ai] : intrusion_config();

            // Record enter timestamp
            enter_timestamps[meta->channel_index][target->track_id][ai] = current_ms;

            // If min_dwell == 0 → alert immediately
            if (config.min_dwell_seconds <= 0) {
                // Check cooldown
                auto &last_ts = last_alert_timestamps[meta->channel_index][target->track_id][ai];
                if (last_ts > 0 && (current_ms - last_ts) < config.cooldown_seconds * 1000.0)
                    continue;

                // Emit INTRUSION_START
                std::vector<int> involve_targets = {target->track_id};
                const auto &polygon = channel_areas[ai];

                std::string img_name = "", vid_name = "";
                if (need_record_image) {
                    img_name = cvedix_utils::time_format(
                        NOW, "intrusion_start_ch" + std::to_string(meta->channel_index) +
                        "_area" + std::to_string(ai) + "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<cvedix_objects::cvedix_image_record_control_meta>(
                        meta->channel_index, img_name, true));
                }
                if (need_record_video) {
                    vid_name = cvedix_utils::time_format(
                        NOW, "intrusion_start_ch" + std::to_string(meta->channel_index) +
                        "_area" + std::to_string(ai) + "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<cvedix_objects::cvedix_video_record_control_meta>(
                        meta->channel_index, vid_name));
                }

                std::string label = "intrusion start";
                if (!config.name.empty()) label += " (" + config.name + ")";

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::INTRUSION_START,
                    meta->channel_index, meta->frame_index, involve_targets,
                    std::vector<cvedix_objects::cvedix_point>(polygon.begin(), polygon.end()),
                    label, img_name, vid_name);

                ba_result->stamp_now();
                ba_result->region_type = "area";
                ba_result->region_name = config.name;
                ba_result->region_id = config.id;
                ba_result->region_index = ai;
                ba_result->populate_target_details(meta->targets, meta->frame, include_target_crops);

                meta->ba_results.push_back(ba_result);
                alerted_entries[meta->channel_index][target->track_id].insert(ai);
                last_ts = current_ms;

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] [area%d] target %d INTRUSION_START",
                    node_name.c_str(), meta->channel_index, ai, target->track_id));
            }
            // If min_dwell > 0 → will be handled in "still inside" check below
        }

        // ── STILL INSIDE: check deferred alerts (min_dwell > 0) ──
        for (int ai : current_areas) {
            if (prev_areas.find(ai) == prev_areas.end()) continue; // just entered, skip
            if (alerted_entries[meta->channel_index][target->track_id].count(ai) > 0) continue; // already alerted

            const auto &config = (ai < (int)configs.size()) ? configs[ai] : intrusion_config();
            if (config.min_dwell_seconds <= 0) continue; // immediate alerts already handled

            // Check if dwell time exceeded
            auto &enter_ts = enter_timestamps[meta->channel_index][target->track_id];
            if (enter_ts.count(ai) == 0) continue;

            double dwell = current_ms - enter_ts[ai];
            if (dwell < config.min_dwell_seconds * 1000.0) continue;

            // Check cooldown
            auto &last_ts = last_alert_timestamps[meta->channel_index][target->track_id][ai];
            if (last_ts > 0 && (current_ms - last_ts) < config.cooldown_seconds * 1000.0)
                continue;

            // Emit deferred INTRUSION_START
            std::vector<int> involve_targets = {target->track_id};
            const auto &polygon = channel_areas[ai];

            std::string img_name = "", vid_name = "";
            if (need_record_image) {
                img_name = cvedix_utils::time_format(
                    NOW, "intrusion_start_ch" + std::to_string(meta->channel_index) +
                    "_area" + std::to_string(ai) + "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<cvedix_objects::cvedix_image_record_control_meta>(
                    meta->channel_index, img_name, true));
            }

            std::string label = "intrusion start (deferred)";
            if (!config.name.empty()) label += " (" + config.name + ")";

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::INTRUSION_START,
                meta->channel_index, meta->frame_index, involve_targets,
                std::vector<cvedix_objects::cvedix_point>(polygon.begin(), polygon.end()),
                label, img_name, vid_name);

            ba_result->stamp_now();
            ba_result->region_type = "area";
            ba_result->region_name = config.name;
            ba_result->region_id = config.id;
            ba_result->region_index = ai;
            ba_result->populate_target_details(meta->targets, meta->frame, include_target_crops);

            meta->ba_results.push_back(ba_result);
            alerted_entries[meta->channel_index][target->track_id].insert(ai);
            last_ts = current_ms;

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] [area%d] target %d INTRUSION_START (deferred %.0fs)",
                node_name.c_str(), meta->channel_index, ai, target->track_id,
                dwell / 1000.0));
        }

        // ── EXIT: in previous but not in current ──
        for (int ai : prev_areas) {
            if (current_areas.find(ai) != current_areas.end()) continue; // still inside

            const auto &config = (ai < (int)configs.size()) ? configs[ai] : intrusion_config();

            // Only emit INTRUSION_END if we emitted INTRUSION_START
            if (alerted_entries[meta->channel_index][target->track_id].count(ai) == 0) {
                // Clean up enter timestamp for unemitted entries
                enter_timestamps[meta->channel_index][target->track_id].erase(ai);
                continue;
            }

            std::vector<int> involve_targets = {target->track_id};
            const auto &polygon = channel_areas[ai];

            std::string img_name = "", vid_name = "";
            if (need_record_image) {
                img_name = cvedix_utils::time_format(
                    NOW, "intrusion_end_ch" + std::to_string(meta->channel_index) +
                    "_area" + std::to_string(ai) + "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<cvedix_objects::cvedix_image_record_control_meta>(
                    meta->channel_index, img_name, true));
            }
            if (need_record_video) {
                vid_name = cvedix_utils::time_format(
                    NOW, "intrusion_end_ch" + std::to_string(meta->channel_index) +
                    "_area" + std::to_string(ai) + "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<cvedix_objects::cvedix_video_record_control_meta>(
                    meta->channel_index, vid_name));
            }

            std::string label = "intrusion end";
            if (!config.name.empty()) label += " (" + config.name + ")";

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::INTRUSION_END,
                meta->channel_index, meta->frame_index, involve_targets,
                std::vector<cvedix_objects::cvedix_point>(polygon.begin(), polygon.end()),
                label, img_name, vid_name);

            ba_result->stamp_now();
            ba_result->region_type = "area";
            ba_result->region_name = config.name;
            ba_result->region_id = config.id;
            ba_result->region_index = ai;
            ba_result->populate_target_details(meta->targets, meta->frame, include_target_crops);

            // Compute event duration
            auto &enter_ts = enter_timestamps[meta->channel_index][target->track_id];
            if (enter_ts.count(ai) > 0) {
                ba_result->event_duration_ms = current_ms - enter_ts[ai];
                enter_ts.erase(ai);
            }

            meta->ba_results.push_back(ba_result);
            alerted_entries[meta->channel_index][target->track_id].erase(ai);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] [area%d] target %d INTRUSION_END (duration: %.0f ms)",
                node_name.c_str(), meta->channel_index, ai, target->track_id,
                ba_result->event_duration_ms));
        }

        // Update previous status
        prev_status[target->track_id] = current_areas;
        last_seen_frame[meta->channel_index][target->track_id] = meta->frame_index;
    }

    // Prune inactive tracks
    int fps = (meta->fps > 0) ? meta->fps : 25;
    int frames_to_keep = fps * inactive_timeout_seconds;
    auto &ch_prev = previous_area_status[meta->channel_index];
    for (auto it = ch_prev.begin(); it != ch_prev.end();) {
        int tid = it->first;
        int last = 0;
        if (last_seen_frame[meta->channel_index].count(tid) > 0)
            last = last_seen_frame[meta->channel_index][tid];

        if (meta->frame_index - last > frames_to_keep) {
            last_seen_frame[meta->channel_index].erase(tid);
            enter_timestamps[meta->channel_index].erase(tid);
            last_alert_timestamps[meta->channel_index].erase(tid);
            alerted_entries[meta->channel_index].erase(tid);
            it = ch_prev.erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

} // namespace cvedix_nodes
