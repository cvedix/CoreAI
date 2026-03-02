#include "cvedix_ba_line_speed_estimation_node.h"

namespace cvedix_nodes {

cvedix_ba_line_speed_estimation_node::cvedix_ba_line_speed_estimation_node(
    std::string node_name,
    std::map<int, std::pair<cvedix_objects::cvedix_line, cvedix_objects::cvedix_line>> line_pairs,
    std::map<int, double> pixel_to_meter,
    double speed_limit_kmh,
    bool need_record_image,
    bool need_record_video)
    : cvedix_node(node_name),
      all_line_pairs(line_pairs),
      all_pixel_to_meter(pixel_to_meter),
      speed_limit_kmh(speed_limit_kmh),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_line_speed_estimation_node::~cvedix_ba_line_speed_estimation_node() {
    deinitialized();
}

std::string cvedix_ba_line_speed_estimation_node::to_string() {
    std::stringstream ss;
    ss << "speed_estimation(limit=" << speed_limit_kmh << "km/h, channels=[";
    for (const auto& p : all_line_pairs) {
        ss << p.first << " ";
    }
    ss << "])";
    return ss.str();
}

void cvedix_ba_line_speed_estimation_node::set_fps(int f) {
    std::lock_guard<std::mutex> lock(config_mutex);
    fps = f;
}

void cvedix_ba_line_speed_estimation_node::set_speed_limit(double limit_kmh) {
    std::lock_guard<std::mutex> lock(config_mutex);
    speed_limit_kmh = limit_kmh;
    CVEDIX_INFO(cvedix_utils::string_format("[%s] Speed limit updated to %.1f km/h",
                                            node_name.c_str(), limit_kmh));
}

bool cvedix_ba_line_speed_estimation_node::at_1_side_of_line(
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
cvedix_ba_line_speed_estimation_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_line_pairs.count(ch) == 0) return meta;

    // Use fps from meta if available
    int current_fps = (meta->fps > 0) ? meta->fps : fps;
    if (current_fps <= 0) current_fps = 30;

    const auto& line1 = all_line_pairs[ch].first;
    const auto& line2 = all_line_pairs[ch].second;
    double px_to_m = all_pixel_to_meter.count(ch) > 0 ? all_pixel_to_meter[ch] : 1.0;

    for (auto& target : meta->targets) {
        auto len = target->tracks.size();
        if (len <= 1 || target->track_id < 0) continue;

        auto curr = target->tracks[len - 1].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
        auto prev = target->tracks[len - 2].track_point(
            cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

        int tid = target->track_id;

        // Check line1 crossing (entry)
        bool curr_side1 = at_1_side_of_line(curr, line1);
        bool prev_side1 = at_1_side_of_line(prev, line1);
        if (curr_side1 != prev_side1) {
            if (line1_cross_frame[ch].count(tid) == 0) {
                line1_cross_frame[ch][tid] = meta->frame_index;
            }
        }

        // Check line2 crossing (exit)
        bool curr_side2 = at_1_side_of_line(curr, line2);
        bool prev_side2 = at_1_side_of_line(prev, line2);
        if (curr_side2 != prev_side2) {
            if (line1_cross_frame[ch].count(tid) > 0 &&
                (!alerted_tracks[ch].count(tid) || !alerted_tracks[ch][tid])) {

                int entry_frame = line1_cross_frame[ch][tid];
                int frame_diff = meta->frame_index - entry_frame;
                if (frame_diff <= 0) frame_diff = 1;

                double time_seconds = static_cast<double>(frame_diff) / current_fps;

                // Calculate pixel distance between line centers
                auto line1_center = cvedix_objects::cvedix_point(
                    (line1.start.x + line1.end.x) / 2,
                    (line1.start.y + line1.end.y) / 2);
                auto line2_center = cvedix_objects::cvedix_point(
                    (line2.start.x + line2.end.x) / 2,
                    (line2.start.y + line2.end.y) / 2);
                double pixel_dist = line1_center.distance_with(line2_center);
                double meter_dist = pixel_dist * px_to_m;

                // Speed in km/h
                double speed_kmh = (meter_dist / time_seconds) * 3.6;

                // Create BA result
                std::string label = cvedix_utils::string_format(
                    "speed: %.1f km/h", speed_kmh);
                std::vector<int> involve_targets = {tid};
                std::vector<cvedix_objects::cvedix_point> involve_region = {
                    line1.start, line1.end, line2.start, line2.end};

                std::string image_file = "";
                std::string video_file = "";

                bool is_violation = speed_kmh > speed_limit_kmh;

                if (is_violation) {
                    label += " [VIOLATION]";

                    if (need_record_image) {
                        image_file = cvedix_utils::time_format(
                            NOW, "speed_ch" + std::to_string(ch) +
                                     "__<year><mon><day><hour><min><sec><mili>");
                        auto ctrl = std::make_shared<
                            cvedix_objects::cvedix_image_record_control_meta>(
                            ch, image_file, true);
                        pendding_meta(ctrl);
                    }

                    if (need_record_video) {
                        video_file = cvedix_utils::time_format(
                            NOW, "speed_ch" + std::to_string(ch) +
                                     "__<year><mon><day><hour><min><sec><mili>");
                        auto ctrl = std::make_shared<
                            cvedix_objects::cvedix_video_record_control_meta>(
                            ch, video_file);
                        pendding_meta(ctrl);
                    }
                }

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::SPEED, ch,
                    meta->frame_index, involve_targets, involve_region, label,
                    image_file, video_file);
                meta->ba_results.push_back(ba_result);

                // Add speed label to target's secondary_labels for OSD display
                std::string speed_label = cvedix_utils::string_format("%.0f km/h", speed_kmh);
                if (is_violation) {
                    speed_label = "[!] " + speed_label;
                }
                target->secondary_labels.push_back(speed_label);

                // Store speed for persistent display
                track_speeds[ch][tid] = speed_label;

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] track %d %s (%.1f m in %.2f s)",
                    node_name.c_str(), ch, tid, label.c_str(),
                    meter_dist, time_seconds));

                alerted_tracks[ch][tid] = true;
                line1_cross_frame[ch].erase(tid);
            }
        }

        // Show stored speed on targets that already crossed
        if (track_speeds[ch].count(tid) > 0 && target->secondary_labels.empty()) {
            target->secondary_labels.push_back(track_speeds[ch][tid]);
        }
    }

    // Prune old entries (tracks that disappeared)
    for (auto& [channel, frame_map] : line1_cross_frame) {
        for (auto it = frame_map.begin(); it != frame_map.end();) {
            if (meta->frame_index - it->second > current_fps * 60) {
                alerted_tracks[channel].erase(it->first);
                track_speeds[channel].erase(it->first);
                it = frame_map.erase(it);
            } else {
                ++it;
            }
        }
    }

    return meta;
}

} // namespace cvedix_nodes
