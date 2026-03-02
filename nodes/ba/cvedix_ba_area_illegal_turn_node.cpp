#include "cvedix_ba_area_illegal_turn_node.h"

namespace cvedix_nodes {

cvedix_ba_area_illegal_turn_node::cvedix_ba_area_illegal_turn_node(
    std::string node_name,
    std::map<int, illegal_turn_config> configs,
    bool need_record_image,
    bool need_record_video)
    : cvedix_node(node_name),
      all_configs(configs),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_illegal_turn_node::~cvedix_ba_area_illegal_turn_node() {
    deinitialized();
}

std::string cvedix_ba_area_illegal_turn_node::to_string() {
    std::stringstream ss;
    ss << "illegal_turn(channels=[";
    for (const auto& p : all_configs) {
        ss << p.first << ":" << p.second.boundary_lines.size() << "arms ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_area_illegal_turn_node::is_inside_polygon(
    const cvedix_objects::cvedix_point& p,
    const std::vector<cvedix_objects::cvedix_point>& polygon) const {
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

bool cvedix_ba_area_illegal_turn_node::at_1_side_of_line(
    cvedix_objects::cvedix_point p, cvedix_objects::cvedix_line line) const {
    auto p1 = line.start;
    auto p2 = line.end;
    if (p1.x == p2.x) return p.x < p1.x;
    if (p1.y == p2.y) return p.y < p1.y;
    if (p2.x < p1.x) { auto tmp = p2; p2 = p1; p1 = tmp; }
    int ret = (p2.y - p.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p2.x - p.x);
    return ret < 0;
}

int cvedix_ba_area_illegal_turn_node::find_crossed_line(
    const cvedix_objects::cvedix_point& curr,
    const cvedix_objects::cvedix_point& prev,
    const std::vector<cvedix_objects::cvedix_line>& lines) const {
    for (size_t i = 0; i < lines.size(); i++) {
        bool curr_side = at_1_side_of_line(curr, lines[i]);
        bool prev_side = at_1_side_of_line(prev, lines[i]);
        if (curr_side != prev_side) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

turn_direction cvedix_ba_area_illegal_turn_node::compute_turn(
    int approach_idx, int exit_idx, int num_lines) const {
    if (approach_idx < 0 || exit_idx < 0 || num_lines < 2) {
        return turn_direction::UNKNOWN;
    }

    if (approach_idx == exit_idx) {
        return turn_direction::U_TURN;
    }

    // Calculate relative position:
    // For a standard 4-way intersection (lines ordered clockwise):
    //   opposite line = (approach + 2) % 4 → STRAIGHT
    //   next line     = (approach + 1) % 4 → RIGHT (clockwise)
    //   prev line     = (approach + 3) % 4 → LEFT  (counter-clockwise)
    int diff = (exit_idx - approach_idx + num_lines) % num_lines;

    if (num_lines == 4) {
        switch (diff) {
            case 1: return turn_direction::RIGHT;
            case 2: return turn_direction::STRAIGHT;
            case 3: return turn_direction::LEFT;
            default: return turn_direction::UNKNOWN;
        }
    } else if (num_lines == 3) {
        // T-junction
        switch (diff) {
            case 1: return turn_direction::RIGHT;
            case 2: return turn_direction::LEFT;
            default: return turn_direction::UNKNOWN;
        }
    } else if (num_lines == 2) {
        // Simple 2-way (U-turn detection only)
        return turn_direction::STRAIGHT;
    }

    // For >4 lines, use angular position
    double angle_per_arm = 360.0 / num_lines;
    double relative_angle = diff * angle_per_arm;

    if (relative_angle > 315 || relative_angle < 45) return turn_direction::U_TURN;
    if (relative_angle >= 45 && relative_angle < 135) return turn_direction::RIGHT;
    if (relative_angle >= 135 && relative_angle < 225) return turn_direction::STRAIGHT;
    if (relative_angle >= 225 && relative_angle <= 315) return turn_direction::LEFT;

    return turn_direction::UNKNOWN;
}

std::string cvedix_ba_area_illegal_turn_node::turn_to_string(turn_direction dir) const {
    switch (dir) {
        case turn_direction::STRAIGHT: return "STRAIGHT";
        case turn_direction::LEFT: return "LEFT";
        case turn_direction::RIGHT: return "RIGHT";
        case turn_direction::U_TURN: return "U_TURN";
        default: return "UNKNOWN";
    }
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_area_illegal_turn_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_configs.count(ch) == 0) return meta;

    const auto& config = all_configs[ch];
    if (config.intersection_area.size() < 3 || config.boundary_lines.empty()) {
        return meta;
    }

    for (auto& target : meta->targets) {
        auto len = target->tracks.size();
        if (len <= 1 || target->track_id < 0) continue;

        int tid = target->track_id;

        // Skip already alerted
        if (alerted_tracks[ch].count(tid) > 0) continue;

        auto curr = target->tracks[len - 1].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
        auto prev = target->tracks[len - 2].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

        bool currently_inside = is_inside_polygon(curr, config.intersection_area);
        auto& state = vehicle_states[ch][tid];

        if (currently_inside && !state.was_inside) {
            // Just entered intersection → record approach line
            int crossed = find_crossed_line(curr, prev, config.boundary_lines);
            if (crossed >= 0) {
                state.approach_line = crossed;
            }
            state.frames_inside = 1;
        } else if (currently_inside) {
            state.frames_inside++;
        } else if (!currently_inside && state.was_inside) {
            // Just exited intersection → determine exit line and turn direction
            if (state.approach_line >= 0 &&
                state.frames_inside >= config.min_frames_inside) {

                int exit_line = find_crossed_line(curr, prev, config.boundary_lines);
                if (exit_line >= 0) {
                    turn_direction turn = compute_turn(
                        state.approach_line, exit_line,
                        config.boundary_lines.size());

                    // Check if this turn is allowed
                    bool allowed = true;
                    for (const auto& rule : config.rules) {
                        if (rule.approach_index != state.approach_line) continue;

                        // Check if rule applies to this vehicle class
                        bool applies = rule.vehicle_class_ids.empty() ||
                            rule.vehicle_class_ids.count(target->primary_class_id) > 0;
                        if (!applies) continue;

                        // Check if turn is in allowed set
                        if (rule.allowed_turns.count(turn) == 0) {
                            allowed = false;
                            break;
                        }
                    }

                    if (!allowed && turn != turn_direction::UNKNOWN) {
                        alerted_tracks[ch].insert(tid);

                        std::string label = "illegal turn [" + turn_to_string(turn) + "]";
                        label += " from arm " + std::to_string(state.approach_line);
                        label += " to arm " + std::to_string(exit_line);
                        if (!config.name.empty()) {
                            label += " (" + config.name + ")";
                        }

                        std::vector<int> involve_targets = {tid};
                        std::vector<cvedix_objects::cvedix_point> involve_region =
                            config.intersection_area;

                        std::string image_file = "";
                        std::string video_file = "";

                        if (need_record_image) {
                            image_file = cvedix_utils::time_format(
                                NOW, "illegal_turn_ch" + std::to_string(ch) +
                                         "__<year><mon><day><hour><min><sec><mili>");
                            pendding_meta(std::make_shared<
                                cvedix_objects::cvedix_image_record_control_meta>(
                                ch, image_file, true));
                        }

                        if (need_record_video) {
                            video_file = cvedix_utils::time_format(
                                NOW, "illegal_turn_ch" + std::to_string(ch) +
                                         "__<year><mon><day><hour><min><sec><mili>");
                            pendding_meta(std::make_shared<
                                cvedix_objects::cvedix_video_record_control_meta>(
                                ch, video_file));
                        }

                        auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                            cvedix_objects::cvedix_ba_type::ILLEGAL_TURN, ch,
                            meta->frame_index, involve_targets, involve_region,
                            label, image_file, video_file);
                        meta->ba_results.push_back(ba_result);

                        CVEDIX_INFO(cvedix_utils::string_format(
                            "[%s] [ch%d] track %d: %s",
                            node_name.c_str(), ch, tid, label.c_str()));
                    }
                }
            }
            // Reset state
            state.approach_line = -1;
            state.frames_inside = 0;
        }

        state.was_inside = currently_inside;
    }

    // Prune disappeared tracks
    for (auto it = vehicle_states[ch].begin(); it != vehicle_states[ch].end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == it->first) { found = true; break; }
        }
        if (!found) {
            alerted_tracks[ch].erase(it->first);
            it = vehicle_states[ch].erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

} // namespace cvedix_nodes
