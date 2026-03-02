#include "cvedix_ba_line_red_light_violation_node.h"

namespace cvedix_nodes {

cvedix_ba_line_red_light_violation_node::cvedix_ba_line_red_light_violation_node(
    std::string node_name,
    std::map<int, red_light_config> configs,
    bool need_record_image,
    bool need_record_video)
    : cvedix_node(node_name),
      all_configs(configs),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
    // Initialize all channels to GREEN
    for (const auto& p : configs) {
        signal_states[p.first] = traffic_signal_state::GREEN;
    }
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_line_red_light_violation_node::~cvedix_ba_line_red_light_violation_node() {
    deinitialized();
}

std::string cvedix_ba_line_red_light_violation_node::to_string() {
    std::stringstream ss;
    ss << "red_light_violation(channels=[";
    for (const auto& p : all_configs) {
        ss << p.first << " ";
    }
    ss << "])";
    return ss.str();
}

void cvedix_ba_line_red_light_violation_node::set_signal_state(
    int channel_id, traffic_signal_state state) {
    std::lock_guard<std::mutex> lock(config_mutex);
    auto prev = signal_states[channel_id];
    signal_states[channel_id] = state;

    if (state == traffic_signal_state::RED && prev != traffic_signal_state::RED) {
        // Record the frame when RED started (will be set on next handle_frame_meta)
        red_start_frame[channel_id] = -1; // sentinel: set on next frame
        // Clear previous state
        crossed_stop_line[channel_id].clear();
        alerted_red_light[channel_id].clear();
        alerted_stop_line[channel_id].clear();
    }

    std::string state_str;
    switch (state) {
        case traffic_signal_state::GREEN: state_str = "GREEN"; break;
        case traffic_signal_state::YELLOW: state_str = "YELLOW"; break;
        case traffic_signal_state::RED: state_str = "RED"; break;
        default: state_str = "UNKNOWN"; break;
    }
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Signal state for channel %d set to %s",
        node_name.c_str(), channel_id, state_str.c_str()));
}

traffic_signal_state cvedix_ba_line_red_light_violation_node::get_signal_state(
    int channel_id) const {
    if (signal_states.count(channel_id) == 0) {
        return traffic_signal_state::UNKNOWN;
    }
    return signal_states.at(channel_id);
}

void cvedix_ba_line_red_light_violation_node::set_fps(int f) {
    std::lock_guard<std::mutex> lock(config_mutex);
    fps = f;
}

bool cvedix_ba_line_red_light_violation_node::at_1_side_of_line(
    cvedix_objects::cvedix_point p, cvedix_objects::cvedix_line line) {
    auto p1 = line.start;
    auto p2 = line.end;
    if (p1.x == p2.x) return p.x < p1.x;
    if (p1.y == p2.y) return p.y < p1.y;
    if (p2.x < p1.x) { auto tmp = p2; p2 = p1; p1 = tmp; }
    int ret = (p2.y - p.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p2.x - p.x);
    return ret < 0;
}

bool cvedix_ba_line_red_light_violation_node::is_inside_polygon(
    const cvedix_objects::cvedix_point& p,
    const std::vector<cvedix_objects::cvedix_point>& polygon) {
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

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_line_red_light_violation_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_configs.count(ch) == 0) return meta;

    // Use fps from meta if available
    int current_fps = (meta->fps > 0) ? meta->fps : fps;
    if (current_fps <= 0) current_fps = 30;

    // Only process when signal is RED
    if (signal_states.count(ch) == 0 ||
        signal_states[ch] != traffic_signal_state::RED) {
        return meta;
    }

    const auto& config = all_configs[ch];

    // Set red start frame on first frame after state change
    if (red_start_frame[ch] < 0) {
        red_start_frame[ch] = meta->frame_index;
    }

    // Grace period check
    int grace_frames = static_cast<int>(config.grace_period_seconds * current_fps);
    bool in_grace = (meta->frame_index - red_start_frame[ch]) < grace_frames;

    for (auto& target : meta->targets) {
        auto len = target->tracks.size();
        if (len <= 1 || target->track_id < 0) continue;

        int tid = target->track_id;

        // Class filter
        if (!config.vehicle_class_ids.empty() &&
            config.vehicle_class_ids.count(target->primary_class_id) == 0) {
            continue;
        }

        auto curr = target->tracks[len - 1].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
        auto prev = target->tracks[len - 2].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

        // Step 1: Check stop line crossing
        bool curr_side = at_1_side_of_line(curr, config.stop_line);
        bool prev_side = at_1_side_of_line(prev, config.stop_line);

        if (curr_side != prev_side && !in_grace) {
            crossed_stop_line[ch].insert(tid);
        }

        // Step 2: Check if already crossed stop line AND now in intersection
        if (crossed_stop_line[ch].count(tid) > 0) {
            bool in_intersection = is_inside_polygon(curr, config.intersection_area);

            if (in_intersection && alerted_red_light[ch].count(tid) == 0) {
                // RED LIGHT violation — most severe
                alerted_red_light[ch].insert(tid);

                std::string label = "RED LIGHT violation";
                if (!config.name.empty()) {
                    label += " (" + config.name + ")";
                }

                std::vector<int> involve_targets = {tid};
                std::vector<cvedix_objects::cvedix_point> involve_region = {
                    config.stop_line.start, config.stop_line.end};
                for (const auto& pt : config.intersection_area) {
                    involve_region.push_back(pt);
                }

                std::string image_file = "";
                std::string video_file = "";

                if (need_record_image) {
                    image_file = cvedix_utils::time_format(
                        NOW, "red_light_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_image_record_control_meta>(
                        ch, image_file, true));
                }

                if (need_record_video) {
                    video_file = cvedix_utils::time_format(
                        NOW, "red_light_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_video_record_control_meta>(
                        ch, video_file));
                }

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::RED_LIGHT, ch,
                    meta->frame_index, involve_targets, involve_region, label,
                    image_file, video_file);
                meta->ba_results.push_back(ba_result);

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] track %d: %s",
                    node_name.c_str(), ch, tid, label.c_str()));

            } else if (!in_intersection &&
                       alerted_stop_line[ch].count(tid) == 0 &&
                       alerted_red_light[ch].count(tid) == 0) {
                // STOP LINE violation — less severe (crossed line but stopped)
                alerted_stop_line[ch].insert(tid);

                std::string label = "stop line violation";
                if (!config.name.empty()) {
                    label += " (" + config.name + ")";
                }

                std::vector<int> involve_targets = {tid};
                std::vector<cvedix_objects::cvedix_point> involve_region = {
                    config.stop_line.start, config.stop_line.end};

                std::string image_file = "";
                std::string video_file = "";

                if (need_record_image) {
                    image_file = cvedix_utils::time_format(
                        NOW, "stop_line_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_image_record_control_meta>(
                        ch, image_file, true));
                }

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::STOP_LINE, ch,
                    meta->frame_index, involve_targets, involve_region, label,
                    image_file, "");
                meta->ba_results.push_back(ba_result);

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] track %d: %s",
                    node_name.c_str(), ch, tid, label.c_str()));
            }
        }
    }

    // Prune disappeared tracks
    for (auto it = crossed_stop_line[ch].begin(); it != crossed_stop_line[ch].end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == *it) { found = true; break; }
        }
        if (!found) {
            alerted_red_light[ch].erase(*it);
            alerted_stop_line[ch].erase(*it);
            it = crossed_stop_line[ch].erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

} // namespace cvedix_nodes
