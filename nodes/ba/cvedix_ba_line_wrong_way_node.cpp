#include "cvedix_ba_line_wrong_way_node.h"

namespace cvedix_nodes {

cvedix_ba_line_wrong_way_node::cvedix_ba_line_wrong_way_node(
    std::string node_name,
    std::map<int, wrong_way_config> configs,
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

cvedix_ba_line_wrong_way_node::~cvedix_ba_line_wrong_way_node() {
    deinitialized();
}

std::string cvedix_ba_line_wrong_way_node::to_string() {
    std::stringstream ss;
    ss << "wrong_way(channels=[";
    for (const auto& p : all_configs) {
        ss << p.first << ":" << p.second.detection_lines.size() << "lines ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_line_wrong_way_node::at_1_side_of_line(
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
cvedix_ba_line_wrong_way_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_configs.count(ch) == 0) return meta;

    const auto& config = all_configs[ch];
    if (config.detection_lines.empty()) return meta;

    for (auto& target : meta->targets) {
        auto len = target->tracks.size();
        if (len <= 1 || target->track_id < 0) continue;

        int tid = target->track_id;

        // Skip exempt vehicle classes
        if (config.exempt_class_ids.count(target->primary_class_id) > 0) continue;

        // Skip already alerted
        if (alerted_tracks[ch].count(tid) > 0) continue;

        auto curr = target->tracks[len - 1].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
        auto prev = target->tracks[len - 2].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

        // Check each detection line
        for (size_t li = 0; li < config.detection_lines.size(); li++) {
            const auto& line = config.detection_lines[li];

            bool curr_side = at_1_side_of_line(curr, line);
            bool prev_side = at_1_side_of_line(prev, line);

            if (curr_side == prev_side) continue; // No crossing

            // Determine crossing direction
            auto detected_dir = (prev_side && !curr_side)
                ? cvedix_objects::cvedix_ba_direct_type::IN
                : cvedix_objects::cvedix_ba_direct_type::OUT;

            // Check if wrong direction
            if (config.allowed_direction != cvedix_objects::cvedix_ba_direct_type::BOTH &&
                detected_dir != config.allowed_direction) {
                wrong_cross_count[ch][tid]++;
            }
        }

        // Check if enough wrong-direction crossings to confirm
        if (wrong_cross_count[ch].count(tid) > 0 &&
            wrong_cross_count[ch][tid] >= config.min_lines_crossed) {

            alerted_tracks[ch].insert(tid);

            std::string label = "wrong way";
            if (!config.name.empty()) {
                label += " (" + config.name + ")";
            }

            std::vector<int> involve_targets = {tid};
            std::vector<cvedix_objects::cvedix_point> involve_region;
            for (const auto& line : config.detection_lines) {
                involve_region.push_back(line.start);
                involve_region.push_back(line.end);
            }

            std::string image_file = "";
            std::string video_file = "";

            if (need_record_image) {
                image_file = cvedix_utils::time_format(
                    NOW, "wrong_way_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_image_record_control_meta>(
                    ch, image_file, true));
            }

            if (need_record_video) {
                video_file = cvedix_utils::time_format(
                    NOW, "wrong_way_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_video_record_control_meta>(
                    ch, video_file));
            }

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::WRONG_WAY, ch,
                meta->frame_index, involve_targets, involve_region, label,
                image_file, video_file);
            meta->ba_results.push_back(ba_result);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] track %d: %s (crossed %d wrong-way lines)",
                node_name.c_str(), ch, tid, label.c_str(),
                wrong_cross_count[ch][tid]));
        }
    }

    // Prune tracks that disappeared
    auto& alerts = alerted_tracks[ch];
    for (auto it = alerts.begin(); it != alerts.end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == *it) { found = true; break; }
        }
        if (!found) {
            wrong_cross_count[ch].erase(*it);
            it = alerts.erase(it);
        } else {
            ++it;
        }
    }

    // Prune wrong_cross_count for tracks not in alerted and not visible
    for (auto it = wrong_cross_count[ch].begin(); it != wrong_cross_count[ch].end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == it->first) { found = true; break; }
        }
        if (!found) {
            it = wrong_cross_count[ch].erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

bool cvedix_ba_line_wrong_way_node::set_config(
    int channel_id, const wrong_way_config& config) {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_configs[channel_id] = config;
    wrong_cross_count[channel_id].clear();
    alerted_tracks[channel_id].clear();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Config updated for channel %d: %zu lines",
        node_name.c_str(), channel_id, config.detection_lines.size()));
    return true;
}

} // namespace cvedix_nodes
