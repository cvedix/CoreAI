#include "cvedix_ba_line_illegal_uturn_node.h"

namespace cvedix_nodes {

cvedix_ba_line_illegal_uturn_node::cvedix_ba_line_illegal_uturn_node(
    std::string node_name,
    std::map<int, uturn_config> configs,
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

cvedix_ba_line_illegal_uturn_node::~cvedix_ba_line_illegal_uturn_node() {
    deinitialized();
}

std::string cvedix_ba_line_illegal_uturn_node::to_string() {
    std::stringstream ss;
    ss << "illegal_uturn(channels=[";
    for (const auto& p : all_configs) {
        ss << p.first << " ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_line_illegal_uturn_node::is_inside_polygon(
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

bool cvedix_ba_line_illegal_uturn_node::detect_uturn(
    const std::vector<cvedix_objects::cvedix_rect>& tracks,
    const uturn_config& config) const {

    int window = config.track_window;
    int len = static_cast<int>(tracks.size());
    if (len < window) return false;

    // Compute heading angles from recent track positions
    std::vector<double> angles;
    int start = len - window;

    for (int i = start; i < len - 1; i++) {
        auto p1 = tracks[i].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
        auto p2 = tracks[i + 1].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;
        double dist = std::sqrt(dx * dx + dy * dy);

        // Skip if movement too small (noise)
        if (dist < config.min_movement_per_step) continue;

        double angle = std::atan2(dy, dx); // radians
        angles.push_back(angle);
    }

    if (angles.size() < 3) return false;

    // Compute cumulative angle change
    double total_angle_change = 0.0;
    for (size_t i = 1; i < angles.size(); i++) {
        double diff = angles[i] - angles[i - 1];

        // Normalize to [-π, π]
        while (diff > M_PI) diff -= 2 * M_PI;
        while (diff < -M_PI) diff += 2 * M_PI;

        total_angle_change += diff;
    }

    double threshold_rad = config.angle_threshold_degrees * M_PI / 180.0;
    return std::abs(total_angle_change) > threshold_rad;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_line_illegal_uturn_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_configs.count(ch) == 0) return meta;

    const auto& config = all_configs[ch];

    for (auto& target : meta->targets) {
        if (target->track_id < 0 || target->tracks.empty()) continue;

        int tid = target->track_id;

        // Skip already alerted
        if (alerted_tracks[ch].count(tid) > 0) continue;

        // Class filter
        if (!config.vehicle_class_ids.empty() &&
            config.vehicle_class_ids.count(target->primary_class_id) == 0) {
            continue;
        }

        // Zone filter
        if (!config.zone_polygon.empty()) {
            auto point = target->tracks.back().track_point(
                cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
            if (!is_inside_polygon(point, config.zone_polygon)) {
                continue;
            }
        }

        // Check for U-turn in trajectory
        if (detect_uturn(target->tracks, config)) {
            alerted_tracks[ch].insert(tid);

            std::string label = "illegal U-turn";
            if (!config.name.empty()) {
                label += " (" + config.name + ")";
            }

            std::vector<int> involve_targets = {tid};
            std::vector<cvedix_objects::cvedix_point> involve_region =
                config.zone_polygon;

            std::string image_file = "";
            std::string video_file = "";

            if (need_record_image) {
                image_file = cvedix_utils::time_format(
                    NOW, "uturn_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_image_record_control_meta>(
                    ch, image_file, true));
            }

            if (need_record_video) {
                video_file = cvedix_utils::time_format(
                    NOW, "uturn_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_video_record_control_meta>(
                    ch, video_file));
            }

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::ILLEGAL_UTURN, ch,
                meta->frame_index, involve_targets, involve_region, label,
                image_file, video_file);
            meta->ba_results.push_back(ba_result);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] track %d: %s",
                node_name.c_str(), ch, tid, label.c_str()));
        }
    }

    // Prune disappeared tracks
    for (auto it = alerted_tracks[ch].begin(); it != alerted_tracks[ch].end();) {
        bool found = false;
        for (const auto& t : meta->targets) {
            if (t->track_id == *it) { found = true; break; }
        }
        if (!found) {
            it = alerted_tracks[ch].erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

bool cvedix_ba_line_illegal_uturn_node::set_config(
    int channel_id, const uturn_config& config) {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_configs[channel_id] = config;
    alerted_tracks[channel_id].clear();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Config updated for channel %d",
        node_name.c_str(), channel_id));
    return true;
}

} // namespace cvedix_nodes
