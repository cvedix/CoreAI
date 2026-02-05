#include "cvedix_ba_loitering_node.h"
 
namespace cvedix_nodes {
 
cvedix_ba_loitering_node::cvedix_ba_loitering_node(
        std::string node_name,
        std::map<int, cvedix_objects::cvedix_rect> rois,
        std::map<int, double> alarm_seconds,
        int fps,
        bool need_record_image,
        bool need_record_video)
    : cvedix_node(node_name),
      all_rois(rois),
      all_alarm_seconds(alarm_seconds),
      fps(fps),
      need_record_image(need_record_image),
      need_record_video(need_record_video)
{
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(), to_string().c_str()));
    this->initialized();
}
 
cvedix_ba_loitering_node::~cvedix_ba_loitering_node() {
    deinitialized();
}
 
std::string cvedix_ba_loitering_node::to_string() {
    std::stringstream ss;
    for (auto& p : all_rois) {
        auto& r = p.second;
        ss << "[channel" << p.first << ": "
<< r.x << "," << r.y << " "
<< r.width << "x" << r.height << "]";
    }
    return ss.str();
}
 
bool cvedix_ba_loitering_node::is_inside_roi(
        int channel_id,
        const cvedix_objects::cvedix_rect& r) const
{
    if (all_rois.count(channel_id) == 0) {
        return false;
    }
 
    // ❗ cvedix_rect is NOT const-correct → copy
    auto roi  = all_rois.at(channel_id);
    auto rect = r;
 
    return roi.contains(rect.track_point());
}
 
std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_loitering_node::handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta)
{
    // if need applied on current channel or not
    if (all_rois.count(meta->channel_index) == 0) {
        return meta;
    }
 
    auto channel_id = meta->channel_index;
    auto& ctx = all_channels[channel_id];
 
    // alarm threshold (default = 10s)
    double alarm_s = 10.0;
    if (all_alarm_seconds.count(channel_id) > 0) {
        alarm_s = all_alarm_seconds[channel_id];
    }
 
    // time update (fallback by fps)
    const double dt = (fps > 0) ? (1.0 / fps) : 0.033;
    ctx.now_sec += dt;
 
    std::vector<int> involve_targets;
 
    // only cvedix_frame_target
    for (auto& target : meta->targets) {
        if (!target || target->track_id < 0) continue;
 
        auto rect = target->get_rect();
        bool inside = is_inside_roi(channel_id, rect);
 
        auto& st = ctx.by_track_id[target->track_id];
        st.last_seen_ts = ctx.now_sec;
 
        if (inside) {
            if (!st.inside) {
                st.inside = true;
                st.enter_ts = ctx.now_sec;
                st.alarmed = false;
            } else {
                double dwell = ctx.now_sec - st.enter_ts;
                if (dwell >= alarm_s && !st.alarmed) {
                    involve_targets.push_back(target->track_id);
                    st.alarmed = true;
                }
            }
        } else {
            st.inside = false;
            st.enter_ts = 0.0;
            st.alarmed = false;
        }
    }
 
    // cleanup expired tracks
    for (auto it = ctx.by_track_id.begin(); it != ctx.by_track_id.end();) {
        if (ctx.now_sec - it->second.last_seen_ts > expire_seconds)
            it = ctx.by_track_id.erase(it);
        else
            ++it;
    }
 
    // ===============================
    // found loitering
    // ===============================
    if (!involve_targets.empty()) {
 
        std::string image_file_name_without_ext = "";
        std::string video_file_name_without_ext = "";
 
        // send image record control meta
        if (need_record_image) {
            image_file_name_without_ext =
                cvedix_utils::time_format(NOW, "loitering_image__<year><mon><day><hour><min><sec><mili>");
            auto image_record_control_meta =
                std::make_shared<cvedix_objects::cvedix_image_record_control_meta>(
                    meta->channel_index,
                    image_file_name_without_ext,
                    true);
            pendding_meta(image_record_control_meta);
        }
 
        // send video record control meta
        if (need_record_video) {
            video_file_name_without_ext =
                cvedix_utils::time_format(NOW, "loitering_video__<year><mon><day><hour><min><sec><mili>");
            auto video_record_control_meta =
                std::make_shared<cvedix_objects::cvedix_video_record_control_meta>(
                    meta->channel_index,
                    video_file_name_without_ext);
            pendding_meta(video_record_control_meta);
        }
 
        // ROI → region points
        std::vector<cvedix_objects::cvedix_point> involve_region;
        auto& roi = all_rois[channel_id];
        involve_region.emplace_back(roi.x, roi.y);
        involve_region.emplace_back(roi.x + roi.width, roi.y);
        involve_region.emplace_back(roi.x + roi.width, roi.y + roi.height);
        involve_region.emplace_back(roi.x, roi.y + roi.height);
 
        auto ba_result =
            std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::STOP,
                meta->channel_index,
                meta->frame_index,
                involve_targets,
                involve_region,
                "loitering",
                image_file_name_without_ext,
                video_file_name_without_ext);
 
        meta->ba_results.push_back(ba_result);
 
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] [channel %d] has found loitering targets: [%zu]",
            node_name.c_str(),
            meta->channel_index,
            involve_targets.size()));
 
        if (need_record_image || need_record_video) {
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [channel %d] image & video record file names are: [%s & %s]",
                node_name.c_str(),
                meta->channel_index,
                image_file_name_without_ext.c_str(),
                video_file_name_without_ext.c_str()));
        }
    }
 
    return meta;
}
 
} // namespace cvedix_nodes