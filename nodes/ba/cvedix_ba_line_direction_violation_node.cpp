#include "cvedix_ba_line_direction_violation_node.h"

namespace cvedix_nodes {

cvedix_ba_line_direction_violation_node::cvedix_ba_line_direction_violation_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_line>> lines,
    std::map<int, std::vector<cvedix_objects::cvedix_ba_direct_type>> allowed_directions,
    bool need_record_image,
    bool need_record_video)
    : cvedix_node(node_name),
      all_lines(lines),
      all_allowed_dirs(allowed_directions),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_line_direction_violation_node::~cvedix_ba_line_direction_violation_node() {
    deinitialized();
}

std::string cvedix_ba_line_direction_violation_node::to_string() {
    std::stringstream ss;
    ss << "direction_violation(channels=[";
    for (const auto& p : all_lines) {
        ss << p.first << ":" << p.second.size() << "lines ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_line_direction_violation_node::at_1_side_of_line(
    cvedix_objects::cvedix_point p, cvedix_objects::cvedix_line line) {
    auto p1 = line.start;
    auto p2 = line.end;

    if (p1.x == p2.x) return p.x < p1.x;
    if (p1.y == p2.y) return p.y < p1.y;

    if (p2.x < p1.x) { auto tmp = p2; p2 = p1; p1 = tmp; }

    int ret = (p2.y - p.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p2.x - p.x);
    return ret < 0;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_line_direction_violation_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_lines.count(ch) == 0 || all_lines[ch].empty()) return meta;

    const auto& channel_lines = all_lines[ch];

    for (auto& target : meta->targets) {
        auto len = target->tracks.size();
        if (len <= 1 || target->track_id < 0) continue;

        auto curr = target->tracks[len - 1].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
        auto prev = target->tracks[len - 2].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

        int tid = target->track_id;

        for (size_t li = 0; li < channel_lines.size(); li++) {
            const auto& line = channel_lines[li];

            bool curr_side = at_1_side_of_line(curr, line);
            bool prev_side = at_1_side_of_line(prev, line);

            if (curr_side == prev_side) continue; // No crossing

            // Determine crossing direction
            auto detected_dir = (prev_side && !curr_side)
                ? cvedix_objects::cvedix_ba_direct_type::IN
                : cvedix_objects::cvedix_ba_direct_type::OUT;

            // Get allowed direction for this line
            auto allowed = cvedix_objects::cvedix_ba_direct_type::BOTH;
            if (all_allowed_dirs.count(ch) > 0 && li < all_allowed_dirs[ch].size()) {
                allowed = all_allowed_dirs[ch][li];
            }

            // If allowed is BOTH, no violation possible
            if (allowed == cvedix_objects::cvedix_ba_direct_type::BOTH) continue;

            // Violation: detected direction != allowed direction
            if (detected_dir != allowed) {
                // Check if already alerted
                if (alerted_tracks[ch].count(tid) > 0) continue;
                alerted_tracks[ch].insert(tid);

                std::string dir_label = (detected_dir == cvedix_objects::cvedix_ba_direct_type::IN)
                    ? "IN" : "OUT";
                std::string label = "direction violation [" + dir_label + "] line " + std::to_string(li);

                std::vector<int> involve_targets = {tid};
                std::vector<cvedix_objects::cvedix_point> involve_region = {line.start, line.end};

                std::string image_file = "";
                std::string video_file = "";

                if (need_record_image) {
                    image_file = cvedix_utils::time_format(
                        NOW, "dir_violation_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_image_record_control_meta>(
                        ch, image_file, true));
                }

                if (need_record_video) {
                    video_file = cvedix_utils::time_format(
                        NOW, "dir_violation_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_video_record_control_meta>(
                        ch, video_file));
                }

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::DIRECTION, ch,
                    meta->frame_index, involve_targets, involve_region, label,
                    image_file, video_file);
                meta->ba_results.push_back(ba_result);

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] track %d: %s",
                    node_name.c_str(), ch, tid, label.c_str()));
            }
        }
    }

    // Prune alerted tracks that are no longer visible
    auto& alerts = alerted_tracks[ch];
    for (auto it = alerts.begin(); it != alerts.end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == *it) { found = true; break; }
        }
        if (!found) it = alerts.erase(it);
        else ++it;
    }

    return meta;
}

} // namespace cvedix_nodes
