#include "cvedix_ba_area_parking_violation_node.h"
#include <cmath>

namespace cvedix_nodes {

cvedix_ba_area_parking_violation_node::cvedix_ba_area_parking_violation_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_point>> zones,
    std::map<int, parking_zone_config> configs,
    int fps, bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_zones(zones), all_configs(configs),
      fps(fps), need_record_image(need_record_image),
      need_record_video(need_record_video), check_interval(10) {
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_parking_violation_node::cvedix_ba_area_parking_violation_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_point>> zones,
    double allowed_seconds, int fps,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_zones(zones),
      fps(fps), need_record_image(need_record_image),
      need_record_video(need_record_video), check_interval(10) {
    for (auto& p : zones) {
        all_configs[p.first] = parking_zone_config(allowed_seconds);
    }
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_parking_violation_node::~cvedix_ba_area_parking_violation_node() {
    deinitialized();
}

std::string cvedix_ba_area_parking_violation_node::to_string() {
    std::lock_guard<std::mutex> lock(config_mutex);
    std::stringstream ss;
    ss << "parking_violation(channels=[";
    for (auto& p : all_zones) {
        double sec = 30.0;
        if (all_configs.count(p.first)) sec = all_configs[p.first].allowed_seconds;
        ss << p.first << ":" << sec << "s ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_area_parking_violation_node::point_in_poly(
    const cvedix_objects::cvedix_point& p,
    const std::vector<cvedix_objects::cvedix_point>& poly) const {
    int n = poly.size();
    bool c = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
            (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) /
                        (poly[j].y - poly[i].y) + poly[i].x)) {
            c = !c;
        }
    }
    return c;
}

bool cvedix_ba_area_parking_violation_node::set_zone(
    int channel_id,
    const std::vector<cvedix_objects::cvedix_point>& zone,
    const parking_zone_config& config) {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_zones[channel_id] = zone;
    all_configs[channel_id] = config;
    all_states[channel_id].clear();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Zone set for channel %d: %s (%.0fs)",
        node_name.c_str(), channel_id, config.name.c_str(),
        config.allowed_seconds));
    return true;
}

bool cvedix_ba_area_parking_violation_node::remove_zone(int channel_id) {
    std::lock_guard<std::mutex> lock(config_mutex);
    if (all_zones.count(channel_id) == 0) return false;
    all_zones.erase(channel_id);
    all_configs.erase(channel_id);
    all_states.erase(channel_id);
    return true;
}

void cvedix_ba_area_parking_violation_node::clear_zones() {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_zones.clear();
    all_configs.clear();
    all_states.clear();
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_area_parking_violation_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_zones.count(ch) == 0) return meta;

    int current_fps = (meta->fps > 0) ? meta->fps : fps;
    if (current_fps <= 0) current_fps = 30;

    const auto& zone = all_zones[ch];
    auto config = all_configs.count(ch) > 0
        ? all_configs[ch] : parking_zone_config();

    auto anchor = config.anchor_point;
    float max_mov = config.max_movement;
    double allowed_sec = config.allowed_seconds;
    const auto& class_filter = config.vehicle_class_ids;

    auto& states = all_states[ch];
    std::set<int> visible_ids;

    for (auto& target : meta->targets) {
        if (target->track_id < 0) continue;

        // Class filtering: skip non-vehicle classes if filter is set
        if (!class_filter.empty() &&
            class_filter.count(target->primary_class_id) == 0) {
            continue;
        }

        int tid = target->track_id;
        visible_ids.insert(tid);

        auto pt = cvedix_objects::cvedix_rect(target->x, target->y,
                                               target->width, target->height)
                      .track_point(anchor);

        bool inside = point_in_poly(pt, zone);
        auto& state = states[tid];

        if (inside && !state.inside) {
            // Just entered zone
            state.inside = true;
            state.enter_frame = meta->frame_index;
            state.last_check_frame = meta->frame_index;
            state.last_x = pt.x;
            state.last_y = pt.y;
            state.stopped_frames = 0;
            state.alerted = false;
        } else if (!inside && state.inside) {
            // Left zone — reset
            state.inside = false;
            state.stopped_frames = 0;
            state.alerted = false;
        }

        if (!state.inside) continue;

        // Check movement periodically
        if (meta->frame_index - state.last_check_frame >= check_interval) {
            float dx = pt.x - state.last_x;
            float dy = pt.y - state.last_y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= max_mov) {
                state.stopped_frames += (meta->frame_index - state.last_check_frame);
            } else {
                // Moving — reset stopped counter
                state.stopped_frames = 0;
                state.alerted = false;
            }

            state.last_check_frame = meta->frame_index;
            state.last_x = pt.x;
            state.last_y = pt.y;
        }

        // Check if parked long enough
        double stopped_seconds = static_cast<double>(state.stopped_frames) / current_fps;

        if (stopped_seconds >= allowed_sec && !state.alerted) {
            state.alerted = true;

            std::string zone_name = config.name.empty() ? "no-parking zone" : config.name;
            std::string label = cvedix_utils::string_format(
                "PARKING VIOLATION in %s (%.0fs > %.0fs limit)",
                zone_name.c_str(), stopped_seconds, allowed_sec);

            std::vector<int> involve_targets = {tid};
            std::vector<cvedix_objects::cvedix_point> involve_region(zone.begin(), zone.end());

            std::string image_file = "";
            std::string video_file = "";

            if (need_record_image) {
                image_file = cvedix_utils::time_format(
                    NOW, "parking_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_image_record_control_meta>(
                    ch, image_file, true));
            }

            if (need_record_video) {
                video_file = cvedix_utils::time_format(
                    NOW, "parking_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_video_record_control_meta>(
                    ch, video_file));
            }

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::PARKING, ch,
                meta->frame_index, involve_targets, involve_region, label,
                image_file, video_file);
            meta->ba_results.push_back(ba_result);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] track %d: %s",
                node_name.c_str(), ch, tid, label.c_str()));
        }
    }

    // Prune disappeared tracks
    for (auto it = states.begin(); it != states.end();) {
        if (visible_ids.count(it->first) == 0) {
            it = states.erase(it);
        } else {
            ++it;
        }
    }

    return meta;
}

} // namespace cvedix_nodes
