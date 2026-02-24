#include "cvedix_ba_area_queue_length_node.h"

namespace cvedix_nodes {

cvedix_ba_area_queue_length_node::cvedix_ba_area_queue_length_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
    std::map<int, queue_config> configs,
    int notify_interval_seconds, int fps,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_rois(rois), all_configs(configs),
      notify_interval_seconds(notify_interval_seconds),
      fps(fps), need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_queue_length_node::cvedix_ba_area_queue_length_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
    int queue_threshold, int notify_interval_seconds, int fps,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_rois(rois),
      notify_interval_seconds(notify_interval_seconds),
      fps(fps), need_record_image(need_record_image),
      need_record_video(need_record_video) {
    for (auto& p : rois) {
        all_configs[p.first] = queue_config(queue_threshold);
    }
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_queue_length_node::~cvedix_ba_area_queue_length_node() {
    deinitialized();
}

std::string cvedix_ba_area_queue_length_node::to_string() {
    std::lock_guard<std::mutex> lock(config_mutex);
    std::stringstream ss;
    ss << "queue_length(channels=[";
    for (auto& p : all_rois) {
        int thr = 5;
        if (all_configs.count(p.first)) thr = all_configs[p.first].queue_threshold;
        ss << p.first << ":thr=" << thr << " ";
    }
    ss << "], interval=" << notify_interval_seconds << "s)";
    return ss.str();
}

bool cvedix_ba_area_queue_length_node::is_inside_roi(
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
cvedix_ba_area_queue_length_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    if (all_rois.count(ch) == 0) return meta;

    int current_fps = (meta->fps > 0) ? meta->fps : fps;
    if (current_fps <= 0) current_fps = 30;

    int threshold = 5;
    if (all_configs.count(ch)) threshold = all_configs[ch].queue_threshold;

    auto anchor = cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM;
    if (all_configs.count(ch)) anchor = all_configs[ch].anchor_point;

    // Count targets inside ROI
    int count = 0;
    std::vector<int> inside_ids;

    for (auto& target : meta->targets) {
        if (target->track_id < 0) continue;
        auto pt = cvedix_objects::cvedix_rect(target->x, target->y,
                                               target->width, target->height)
                      .track_point(anchor);
        if (is_inside_roi(ch, pt)) {
            count++;
            inside_ids.push_back(target->track_id);
        }
    }

    // Check if threshold exceeded and enough time since last notify
    if (count >= threshold) {
        int notify_interval_frames = notify_interval_seconds * current_fps;
        bool should_notify = true;

        if (last_notify_frame.count(ch) > 0) {
            if (meta->frame_index - last_notify_frame[ch] < notify_interval_frames) {
                should_notify = false;
            }
        }

        if (should_notify) {
            last_notify_frame[ch] = meta->frame_index;

            std::string area_name = "";
            if (all_configs.count(ch) && !all_configs[ch].name.empty()) {
                area_name = all_configs[ch].name + ": ";
            }

            std::string label = cvedix_utils::string_format(
                "%squeue length %d (threshold %d)", area_name.c_str(), count, threshold);

            const auto& roi = all_rois[ch];
            std::vector<cvedix_objects::cvedix_point> involve_region(roi.begin(), roi.end());

            std::string image_file = "";
            std::string video_file = "";

            if (need_record_image) {
                image_file = cvedix_utils::time_format(
                    NOW, "queue_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_image_record_control_meta>(
                    ch, image_file, true));
            }

            if (need_record_video) {
                video_file = cvedix_utils::time_format(
                    NOW, "queue_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_video_record_control_meta>(
                    ch, video_file));
            }

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::QUEUE, ch,
                meta->frame_index, inside_ids, involve_region, label,
                image_file, video_file);
            meta->ba_results.push_back(ba_result);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] %s",
                node_name.c_str(), ch, label.c_str()));
        }
    }

    return meta;
}

} // namespace cvedix_nodes
