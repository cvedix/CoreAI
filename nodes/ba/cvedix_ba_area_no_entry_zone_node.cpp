#include "cvedix_ba_area_no_entry_zone_node.h"

namespace cvedix_nodes {

cvedix_ba_area_no_entry_zone_node::cvedix_ba_area_no_entry_zone_node(
    std::string node_name,
    std::map<int, no_entry_config> configs,
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

cvedix_ba_area_no_entry_zone_node::~cvedix_ba_area_no_entry_zone_node() {
    deinitialized();
}

std::string cvedix_ba_area_no_entry_zone_node::to_string() {
    std::stringstream ss;
    ss << "no_entry_zone(channels=[";
    for (const auto& p : all_configs) {
        ss << p.first << ":" << p.second.class_schedules.size() << "classes ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_area_no_entry_zone_node::is_inside_polygon(
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

bool cvedix_ba_area_no_entry_zone_node::is_within_schedule(
    const time_schedule& sched, int hour, int minute, int wday) const {
    // Check day of week
    if (!sched.days_of_week.empty() &&
        sched.days_of_week.count(wday) == 0) {
        return false;
    }

    // Convert to minutes for comparison
    int current_mins = hour * 60 + minute;
    int start_mins = sched.start_hour * 60 + sched.start_minute;
    int end_mins = sched.end_hour * 60 + sched.end_minute;

    if (start_mins <= end_mins) {
        // Normal range: 07:00 - 21:00
        return current_mins >= start_mins && current_mins <= end_mins;
    } else {
        // Overnight range: 22:00 - 06:00
        return current_mins >= start_mins || current_mins <= end_mins;
    }
}

bool cvedix_ba_area_no_entry_zone_node::is_restricted_now(
    int class_id,
    const std::map<int, std::vector<time_schedule>>& schedules) const {

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm local_time;
    localtime_r(&time_t_now, &local_time);

    int hour = local_time.tm_hour;
    int minute = local_time.tm_min;
    int wday = local_time.tm_wday; // 0=Sunday

    // If no schedules defined → always restricted for all classes
    if (schedules.empty()) {
        return true;
    }

    // Check if this class_id has restrictions
    if (schedules.count(class_id) == 0) {
        return false; // This class is not restricted
    }

    // Check if current time falls within any schedule for this class
    for (const auto& sched : schedules.at(class_id)) {
        if (is_within_schedule(sched, hour, minute, wday)) {
            return true;
        }
    }

    return false;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_area_no_entry_zone_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_configs.count(ch) == 0) return meta;

    const auto& config = all_configs[ch];
    if (config.zone_polygon.size() < 3) return meta;

    for (auto& target : meta->targets) {
        if (target->tracks.empty() || target->track_id < 0) continue;

        int tid = target->track_id;
        int class_id = target->primary_class_id;

        auto point = target->tracks.back().track_point(config.anchor_point);
        bool in_zone = is_inside_polygon(point, config.zone_polygon);

        if (in_zone && is_restricted_now(class_id, config.class_schedules)) {
            if (alerted_tracks[ch].count(tid) == 0) {
                alerted_tracks[ch].insert(tid);

                std::string label = "no-entry zone violation";
                if (!config.zone_name.empty()) {
                    label += " [" + config.zone_name + "]";
                }
                label += " class=" + target->primary_label;

                std::vector<int> involve_targets = {tid};
                std::vector<cvedix_objects::cvedix_point> involve_region =
                    config.zone_polygon;

                std::string image_file = "";
                std::string video_file = "";

                if (need_record_image) {
                    image_file = cvedix_utils::time_format(
                        NOW, "no_entry_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_image_record_control_meta>(
                        ch, image_file, true));
                }

                if (need_record_video) {
                    video_file = cvedix_utils::time_format(
                        NOW, "no_entry_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_video_record_control_meta>(
                        ch, video_file));
                }

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::NO_ENTRY, ch,
                    meta->frame_index, involve_targets, involve_region, label,
                    image_file, video_file);
                meta->ba_results.push_back(ba_result);

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] track %d: %s",
                    node_name.c_str(), ch, tid, label.c_str()));
            }
        } else if (!in_zone) {
            // Clear alert when vehicle leaves zone (allow re-trigger)
            alerted_tracks[ch].erase(tid);
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

bool cvedix_ba_area_no_entry_zone_node::set_config(
    int channel_id, const no_entry_config& config) {
    std::lock_guard<std::mutex> lock(config_mutex);
    all_configs[channel_id] = config;
    alerted_tracks[channel_id].clear();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Config updated for channel %d",
        node_name.c_str(), channel_id));
    return true;
}

} // namespace cvedix_nodes
