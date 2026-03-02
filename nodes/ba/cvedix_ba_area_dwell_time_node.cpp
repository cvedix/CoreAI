#include "cvedix_ba_area_dwell_time_node.h"
#include <set>

namespace cvedix_nodes {

cvedix_ba_area_dwell_time_node::cvedix_ba_area_dwell_time_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
    std::map<int, dwell_time_config> configs,
    int fps, bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_rois(rois), all_configs(configs),
      fps(fps), need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_dwell_time_node::cvedix_ba_area_dwell_time_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
    double threshold_seconds, int fps,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_rois(rois),
      fps(fps), need_record_image(need_record_image),
      need_record_video(need_record_video) {
    for (auto& p : rois) {
        all_configs[p.first] = dwell_time_config(threshold_seconds);
    }
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_dwell_time_node::~cvedix_ba_area_dwell_time_node() {
    deinitialized();
}

std::string cvedix_ba_area_dwell_time_node::to_string() {
    std::lock_guard<std::mutex> lock(config_mutex);
    std::stringstream ss;
    ss << "dwell_time(channels=[";
    for (auto& p : all_rois) {
        double thr = 30.0;
        if (all_configs.count(p.first)) thr = all_configs[p.first].threshold_seconds;
        ss << p.first << ":" << thr << "s ";
    }
    ss << "])";
    return ss.str();
}

bool cvedix_ba_area_dwell_time_node::is_inside_roi(
    int channel_id, const cvedix_objects::cvedix_point& pt) const {
    if (all_rois.count(channel_id) == 0) return false;
    const auto& roi = all_rois.at(channel_id);
    int nvert = roi.size();
    bool c = false;
    for (int i = 0, j = nvert - 1; i < nvert; j = i++) {
        if (((roi[i].y > pt.y) != (roi[j].y > pt.y)) &&
            (pt.x < (roi[j].x - roi[i].x) * (pt.y - roi[i].y) /
                         (roi[j].y - roi[i].y) + roi[i].x)) {
            c = !c;
        }
    }
    return c;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_area_dwell_time_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_rois.count(ch) == 0) return meta;

    int current_fps = (meta->fps > 0) ? meta->fps : fps;
    if (current_fps <= 0) current_fps = 30;

    double threshold = 30.0;
    if (all_configs.count(ch)) threshold = all_configs[ch].threshold_seconds;

    auto anchor = cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM;
    if (all_configs.count(ch)) anchor = all_configs[ch].anchor_point;

    auto& states = all_states[ch];

    // Track which IDs are visible this frame
    std::set<int> visible_ids;

    for (auto& target : meta->targets) {
        if (target->track_id < 0) continue;
        int tid = target->track_id;
        visible_ids.insert(tid);

        auto pt = cvedix_objects::cvedix_rect(target->x, target->y,
                                               target->width, target->height)
                      .track_point(anchor);
        bool inside = is_inside_roi(ch, pt);

        auto& state = states[tid];

        if (inside && !state.inside) {
            // Just entered
            state.inside = true;
            state.enter_frame = meta->frame_index;
            state.alerted = false;
        } else if (!inside && state.inside) {
            // Just exited
            state.inside = false;
            state.alerted = false;
        }

        if (state.inside && !state.alerted) {
            double dwell_seconds = static_cast<double>(meta->frame_index - state.enter_frame) / current_fps;

            if (dwell_seconds >= threshold) {
                state.alerted = true;

                std::string area_name = "";
                if (all_configs.count(ch) && !all_configs[ch].name.empty()) {
                    area_name = all_configs[ch].name + ": ";
                }

                std::string label = cvedix_utils::string_format(
                    "%sdwell %.1fs", area_name.c_str(), dwell_seconds);

                std::vector<int> involve_targets = {tid};
                const auto& roi = all_rois[ch];
                std::vector<cvedix_objects::cvedix_point> involve_region(roi.begin(), roi.end());

                std::string image_file = "";
                std::string video_file = "";

                if (need_record_image) {
                    image_file = cvedix_utils::time_format(
                        NOW, "dwell_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_image_record_control_meta>(
                        ch, image_file, true));
                }

                if (need_record_video) {
                    video_file = cvedix_utils::time_format(
                        NOW, "dwell_ch" + std::to_string(ch) +
                                 "__<year><mon><day><hour><min><sec><mili>");
                    pendding_meta(std::make_shared<
                        cvedix_objects::cvedix_video_record_control_meta>(
                        ch, video_file));
                }

                auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                    cvedix_objects::cvedix_ba_type::DWELL, ch,
                    meta->frame_index, involve_targets, involve_region, label,
                    image_file, video_file);
                meta->ba_results.push_back(ba_result);

                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] [ch%d] track %d: %s",
                    node_name.c_str(), ch, tid, label.c_str()));
            }
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
