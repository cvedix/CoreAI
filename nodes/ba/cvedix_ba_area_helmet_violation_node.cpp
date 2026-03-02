#include "cvedix_ba_area_helmet_violation_node.h"

namespace cvedix_nodes {

cvedix_ba_area_helmet_violation_node::cvedix_ba_area_helmet_violation_node(
    std::string node_name,
    std::map<int, helmet_config> configs,
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

cvedix_ba_area_helmet_violation_node::~cvedix_ba_area_helmet_violation_node() {
    deinitialized();
}

std::string cvedix_ba_area_helmet_violation_node::to_string() {
    std::stringstream ss;
    ss << "helmet_violation(channels=[";
    for (const auto& p : all_configs) {
        ss << p.first << " ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_area_helmet_violation_node::is_inside_polygon(
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

bool cvedix_ba_area_helmet_violation_node::has_no_helmet(
    const std::shared_ptr<cvedix_objects::cvedix_frame_target>& target,
    const helmet_config& config) const {
    // Check secondary class IDs
    for (int cls_id : target->secondary_class_ids) {
        if (cls_id == config.no_helmet_class_id) {
            return true;
        }
    }
    // Check secondary labels
    for (const auto& label : target->secondary_labels) {
        if (label == config.no_helmet_label) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_area_helmet_violation_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_configs.count(ch) == 0) return meta;

    const auto& config = all_configs[ch];

    for (auto& target : meta->targets) {
        if (target->track_id < 0) continue;

        int tid = target->track_id;

        // Filter: only motorcycle class
        if (config.motorcycle_class_ids.count(target->primary_class_id) == 0) {
            continue;
        }

        // Optional area filter
        if (!config.detection_area.empty() && !target->tracks.empty()) {
            auto point = target->tracks.back().track_point(
                cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
            if (!is_inside_polygon(point, config.detection_area)) {
                continue;
            }
        }

        // Skip already alerted
        if (alerted_tracks[ch].count(tid) > 0) continue;

        // Check helmet status from secondary classifier
        if (has_no_helmet(target, config)) {
            no_helmet_frames[ch][tid]++;

            if (no_helmet_frames[ch][tid] >= config.min_confirm_frames) {
                alerted_tracks[ch].insert(tid);

                std::string label = "helmet violation (no helmet)";
                if (!config.name.empty()) {
                    label += " (" + config.name + ")";
                }

                std::vector<int> involve_targets = {tid};
                std::vector<cvedix_objects::cvedix_point> involve_region;

                std::string image_file = "";
                std::string video_file = "";

                if (need_record_image) {
                    image_file = cvedix_utils::time_format(
                        NOW, "helmet_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_image_record_control_meta>(
                        ch, image_file, true));
                }

                if (need_record_video) {
                    video_file = cvedix_utils::time_format(
                        NOW, "helmet_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_video_record_control_meta>(
                        ch, video_file));
                }

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::HELMET, ch,
                    meta->frame_index, involve_targets, involve_region, label,
                    image_file, video_file);
                meta->ba_results.push_back(ba_result);

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] track %d: %s",
                    node_name.c_str(), ch, tid, label.c_str()));
            }
        } else {
            // Wearing helmet → reset counter
            no_helmet_frames[ch][tid] = 0;
        }
    }

    // Prune disappeared tracks
    for (auto it = no_helmet_frames[ch].begin(); it != no_helmet_frames[ch].end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == it->first) { found = true; break; }
        }
        if (!found) {
            alerted_tracks[ch].erase(it->first);
            it = no_helmet_frames[ch].erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

bool cvedix_ba_area_helmet_violation_node::set_config(
    int channel_id, const helmet_config& config) {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_configs[channel_id] = config;
    no_helmet_frames[channel_id].clear();
    alerted_tracks[channel_id].clear();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Config updated for channel %d",
        node_name.c_str(), channel_id));
    return true;
}

} // namespace cvedix_nodes
