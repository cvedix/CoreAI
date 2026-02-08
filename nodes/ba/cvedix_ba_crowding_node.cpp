#include "cvedix_ba_crowding_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
 
namespace cvedix_nodes {
 
cvedix_ba_crowding_node::cvedix_ba_crowding_node(
                std::string node_name,
                std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
        std::map<int, int> obj_count_thresholds,
        std::map<int, double> alarm_seconds,
        int fps,
        bool need_record_image,
        bool need_record_video)
    : cvedix_node(node_name),
            all_rois(rois),
      all_obj_count_thresholds(obj_count_thresholds),
      all_alarm_seconds(alarm_seconds),
      fps(fps),
      need_record_image(need_record_image),
      need_record_video(need_record_video)
{
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(), to_string().c_str()));
    this->initialized();
}

cvedix_ba_crowding_node::~cvedix_ba_crowding_node() {
    deinitialized();
}

std::string cvedix_ba_crowding_node::to_string() {
    std::stringstream ss;
    for (auto& p : all_rois) {
        auto& poly = p.second;
        ss << "[channel" << p.first << ": polygon(" << poly.size() << ") ";
        for (size_t i = 0; i < poly.size(); ++i) {
            ss << "(" << poly[i].x << "," << poly[i].y << ")";
            if (i + 1 < poly.size()) ss << ",";
        }
        ss << "]";
    }
    return ss.str();
}
 
bool cvedix_ba_crowding_node::is_inside_roi(
        int channel_id,
        const cvedix_objects::cvedix_point& pt) const
{
    if (all_rois.count(channel_id) == 0) {
        return false;
    }

    // Ray-casting point-in-polygon
    const auto& poly = all_rois.at(channel_id);
    bool inside = false;
    size_t n = poly.size();
    if (n < 3) return false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        double xi = poly[i].x, yi = poly[i].y;
        double xj = poly[j].x, yj = poly[j].y;
        bool intersect = ((yi > pt.y) != (yj > pt.y)) &&
            (pt.x < (xj - xi) * (pt.y - yi) / (yj - yi + 1e-12) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}
 
std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_crowding_node::handle_frame_meta(
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
            bool inside = is_inside_roi(channel_id, rect.track_point());

        if (inside) {
            // add track id to ctx if not exist
            if (ctx.by_track_id.count(target->track_id) == 0) {
                loiter_state st{};
                st.inside = true;
                st.enter_ts = ctx.now_sec;
                st.last_seen_ts = ctx.now_sec;
                ctx.by_track_id[target->track_id] = st;
            }
            ctx.by_track_id[target->track_id].inside = true;
            ctx.by_track_id[target->track_id].last_seen_ts = ctx.now_sec;
            involve_targets.push_back(target->track_id);
            // Log
            // CVEDIX_INFO(cvedix_utils::string_format(
            //     "[%s] [channel %d] target track_id=%d is inside ROI",
            //     node_name.c_str(),
            //     meta->channel_index,
            //     target->track_id));
        } else if (ctx.by_track_id.count(target->track_id) != 0) {
            ctx.by_track_id.erase(target->track_id);
        } 
    }
    
    // Clear track_id if not seen for a while (collect keys then erase to avoid
    // invalidating the iterator while iterating)
    std::vector<int> stale_ids;
    for (auto& p : ctx.by_track_id) {
        if (ctx.now_sec - p.second.last_seen_ts > expire_seconds) {
            stale_ids.push_back(p.first);
        }
    }
    for (auto id : stale_ids) {
        ctx.by_track_id.erase(id);
    }

    // Check if crowding condition met
    if (ctx.by_track_id.size() >=
        all_obj_count_thresholds[channel_id]) {
        if (!ctx.alarmed) {
            // Check if loitering time exceeded
            bool all_exceeded = true;
            for (auto& p : ctx.by_track_id) {
                if (ctx.now_sec - p.second.enter_ts < alarm_s) {
                    all_exceeded = false;
                    break;
                }
            }
            if (all_exceeded) {
                ctx.alarmed = true;
            }
        }
    } else {
        ctx.alarmed = false;
    }
 
    // If alarmed, generate ba_result
    if (!involve_targets.empty() && ctx.alarmed) {
 
        std::string image_file_name_without_ext = "";
        std::string video_file_name_without_ext = "";
 
        // send image record control meta
        if (need_record_image) {
            image_file_name_without_ext =
                cvedix_utils::time_format(NOW, "crowding_image__<year><mon><day><hour><min><sec><mili>");
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
                cvedix_utils::time_format(NOW, "crowding_video__<year><mon><day><hour><min><sec><mili>");
            auto video_record_control_meta =
                std::make_shared<cvedix_objects::cvedix_video_record_control_meta>(
                    meta->channel_index,
                    video_file_name_without_ext);
            pendding_meta(video_record_control_meta);
        }
 
        // ROI → region points (copy polygon)
        std::vector<cvedix_objects::cvedix_point> involve_region = all_rois[channel_id];
 
        auto ba_result =
            std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::STOP,
                meta->channel_index,
                meta->frame_index,
                involve_targets,
                involve_region,
                "crowding",
                image_file_name_without_ext,
                video_file_name_without_ext);
 
        meta->ba_results.push_back(ba_result);
 
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] [channel %d] has found crowding targets: [%zu]",
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