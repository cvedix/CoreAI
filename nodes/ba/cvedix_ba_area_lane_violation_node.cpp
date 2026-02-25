#include "cvedix_ba_area_lane_violation_node.h"

namespace cvedix_nodes {

cvedix_ba_area_lane_violation_node::cvedix_ba_area_lane_violation_node(
    std::string node_name,
    std::map<int, std::vector<lane_config>> lane_configs,
    bool need_record_image,
    bool need_record_video)
    : cvedix_node(node_name),
      all_lane_configs(lane_configs),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_lane_violation_node::~cvedix_ba_area_lane_violation_node() {
    deinitialized();
}

std::string cvedix_ba_area_lane_violation_node::to_string() {
    std::stringstream ss;
    ss << "lane_violation(channels=[";
    for (const auto& p : all_lane_configs) {
        ss << p.first << ":" << p.second.size() << "lanes ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_area_lane_violation_node::is_inside_polygon(
    const cvedix_objects::cvedix_point& p,
    const std::vector<cvedix_objects::cvedix_point>& polygon) const {
    // Ray-casting algorithm
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
cvedix_ba_area_lane_violation_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_lane_configs.count(ch) == 0 || all_lane_configs[ch].empty()) {
        return meta;
    }

    const auto& lanes = all_lane_configs[ch];

    for (auto& target : meta->targets) {
        if (target->tracks.empty() || target->track_id < 0) continue;

        int tid = target->track_id;
        int class_id = target->primary_class_id;

        // Get current position using track point
        auto current_bbox = target->tracks.back();

        for (size_t li = 0; li < lanes.size(); li++) {
            const auto& lane = lanes[li];

            auto point = current_bbox.track_point(lane.anchor_point);
            bool in_lane = is_inside_polygon(point, lane.lane_polygon);

            if (in_lane) {
                // Check if vehicle class is NOT allowed in this lane
                bool is_wrong_class = !lane.allowed_class_ids.empty() &&
                    lane.allowed_class_ids.count(class_id) == 0;

                if (is_wrong_class) {
                    frames_in_wrong_lane[ch][tid][li]++;

                    // Check if exceeded threshold and not yet alerted
                    if (frames_in_wrong_lane[ch][tid][li] >= lane.min_frames_in_lane &&
                        alerted_tracks[ch][tid].count(li) == 0) {

                        alerted_tracks[ch][tid].insert(li);

                        std::string label = "lane violation";
                        if (!lane.lane_name.empty()) {
                            label += " [" + lane.lane_name + "]";
                        }
                        label += " class=" + target->primary_label;

                        std::vector<int> involve_targets = {tid};
                        std::vector<cvedix_objects::cvedix_point> involve_region =
                            lane.lane_polygon;

                        std::string image_file = "";
                        std::string video_file = "";

                        if (need_record_image) {
                            image_file = cvedix_utils::time_format(
                                NOW, "lane_violation_ch" + std::to_string(ch) +
                                         "_lane" + std::to_string(li) +
                                         "__<year><mon><day><hour><min><sec><mili>");
                            pendding_meta(std::make_shared<
                                cvedix_objects::cvedix_image_record_control_meta>(
                                ch, image_file, true));
                        }

                        if (need_record_video) {
                            video_file = cvedix_utils::time_format(
                                NOW, "lane_violation_ch" + std::to_string(ch) +
                                         "_lane" + std::to_string(li) +
                                         "__<year><mon><day><hour><min><sec><mili>");
                            pendding_meta(std::make_shared<
                                cvedix_objects::cvedix_video_record_control_meta>(
                                ch, video_file));
                        }

                        auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                            cvedix_objects::cvedix_ba_type::LANE_VIOLATION, ch,
                            meta->frame_index, involve_targets, involve_region,
                            label, image_file, video_file);
                        meta->ba_results.push_back(ba_result);

                        CVEDIX_INFO(cvedix_utils::string_format(
                            "[%s] [ch%d] track %d: %s (in wrong lane for %d frames)",
                            node_name.c_str(), ch, tid, label.c_str(),
                            frames_in_wrong_lane[ch][tid][li]));
                    }
                } else {
                    // Correct class → reset counter for this lane
                    frames_in_wrong_lane[ch][tid][li] = 0;
                }
            } else {
                // Not in this lane → reset counter
                frames_in_wrong_lane[ch][tid][li] = 0;
                // Clear alert so it can re-trigger if vehicle returns
                alerted_tracks[ch][tid].erase(li);
            }
        }
    }

    // Prune disappeared tracks
    for (auto it = frames_in_wrong_lane[ch].begin();
         it != frames_in_wrong_lane[ch].end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == it->first) { found = true; break; }
        }
        if (!found) {
            alerted_tracks[ch].erase(it->first);
            it = frames_in_wrong_lane[ch].erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

bool cvedix_ba_area_lane_violation_node::set_lanes(
    int channel_id, const std::vector<lane_config>& configs) {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_lane_configs[channel_id] = configs;
    frames_in_wrong_lane[channel_id].clear();
    alerted_tracks[channel_id].clear();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Lanes updated for channel %d: %zu lanes",
        node_name.c_str(), channel_id, configs.size()));
    return true;
}

void cvedix_ba_area_lane_violation_node::clear_lanes() {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_lane_configs.clear();
    frames_in_wrong_lane.clear();
    alerted_tracks.clear();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] All lanes cleared", node_name.c_str()));
}

} // namespace cvedix_nodes
