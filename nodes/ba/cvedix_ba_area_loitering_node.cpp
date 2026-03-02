#include "cvedix_ba_area_loitering_node.h"
 
namespace cvedix_nodes {
 
cvedix_ba_area_loitering_node::cvedix_ba_area_loitering_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
        std::map<int, loitering_config> configs,
        int fps,
        bool need_record_image,
        bool need_record_video)
    : cvedix_node(node_name),
      all_rois(rois),
      all_configs(configs),
      fps(fps),
      need_record_image(need_record_image),
      need_record_video(need_record_video)
{
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(), to_string().c_str()));
    this->initialized();
}

cvedix_ba_area_loitering_node::cvedix_ba_area_loitering_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
        int fps,
        bool need_record_image,
        bool need_record_video)
    : cvedix_node(node_name),
      all_rois(rois),
      fps(fps),
      need_record_image(need_record_image),
      need_record_video(need_record_video)
{
    // Initialize default configs for all channels
    for (const auto &channel_pair : all_rois) {
        int channel_id = channel_pair.first;
        all_configs[channel_id] = loitering_config(); // default config
    }
    
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(), to_string().c_str()));
    this->initialized();
}
 
cvedix_ba_area_loitering_node::~cvedix_ba_area_loitering_node() {
    deinitialized();
}
 
std::string cvedix_ba_area_loitering_node::to_string() {
    std::lock_guard<std::mutex> lock(config_mutex);
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
 
bool cvedix_ba_area_loitering_node::is_inside_roi(
        int channel_id,
        const cvedix_objects::cvedix_point& pt) const
{
    std::lock_guard<std::mutex> lock(config_mutex);
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
cvedix_ba_area_loitering_node::handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta)
{
    auto channel_id = meta->channel_index;
    
    // Snapshot configuration with mutex protection
    loitering_config config;
    std::vector<cvedix_objects::cvedix_point> roi_copy;
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        
        // if need applied on current channel or not
        if (all_rois.count(channel_id) == 0) {
            return meta;
        }
        
        // Copy configuration for this channel
        roi_copy = all_rois[channel_id];
        config = (all_configs.count(channel_id) > 0) 
                 ? all_configs[channel_id] 
                 : loitering_config(); // default: alarm=10s
    }
 
    auto& ctx = all_channels[channel_id];
 
    // time update (fallback by fps)
    const double dt = (fps > 0) ? (1.0 / fps) : 0.033;
    ctx.now_sec += dt;
 
    std::vector<int> involve_targets;
 
    // only cvedix_frame_target
    for (auto& target : meta->targets) {
        if (!target || target->track_id < 0) continue;
 
        auto rect = target->get_rect();
        bool inside = is_inside_roi(channel_id, rect.track_point(config.anchor_point));
 
        auto& st = ctx.by_track_id[target->track_id];
        st.last_seen_ts = ctx.now_sec;
 
        if (inside) {
            if (!st.inside) {
                st.inside = true;
                st.enter_ts = ctx.now_sec;
                st.alarmed = false;
            } else {
                double dwell = ctx.now_sec - st.enter_ts;
                if (dwell >= config.alarm_seconds && !st.alarmed) {
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
 
        // ROI → region points (copy polygon)
        std::vector<cvedix_objects::cvedix_point> involve_region = roi_copy;
 
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

bool cvedix_ba_area_loitering_node::set_rois(
    const std::map<int, std::vector<cvedix_objects::cvedix_point>> &rois) {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  all_rois = rois;
  // Clear runtime states for all channels
  all_channels.clear();
  
  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] ROIs replaced at runtime: %s", node_name.c_str(),
      to_string().c_str()));
  
  return true;
}

bool cvedix_ba_area_loitering_node::set_channel_roi(
    int channel_id, const std::vector<cvedix_objects::cvedix_point> &roi) {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  all_rois[channel_id] = roi;
  // Clear runtime state for this channel
  all_channels[channel_id] = channel_ctx();
  
  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Set ROI for channel %d: polygon with %zu points",
      node_name.c_str(), channel_id, roi.size()));
  
  return true;
}

bool cvedix_ba_area_loitering_node::remove_channel_roi(int channel_id) {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  if (all_rois.count(channel_id) == 0) {
    return false;
  }
  
  all_rois.erase(channel_id);
  all_configs.erase(channel_id);
  all_channels.erase(channel_id);
  
  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Removed ROI and config for channel %d", node_name.c_str(), channel_id));
  
  return true;
}

void cvedix_ba_area_loitering_node::clear_rois() {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  all_rois.clear();
  all_configs.clear();
  all_channels.clear();
  
  CVEDIX_INFO(cvedix_utils::string_format("[%s] Cleared all ROIs and config",
                                          node_name.c_str()));
}

std::vector<cvedix_objects::cvedix_point> 
cvedix_ba_area_loitering_node::get_channel_roi(int channel_id) const {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  if (all_rois.count(channel_id) == 0) {
    return std::vector<cvedix_objects::cvedix_point>();
  }
  
  return all_rois.at(channel_id);
}

bool cvedix_ba_area_loitering_node::set_config(int channel_id, const loitering_config &config) {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  all_configs[channel_id] = config;
  
  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Set config for channel %d: alarm=%.2fs, name=%s",
      node_name.c_str(), channel_id, config.alarm_seconds, config.name.c_str()));
  
  return true;
}

loitering_config cvedix_ba_area_loitering_node::get_config(int channel_id) const {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  if (all_configs.count(channel_id) == 0) {
    return loitering_config(); // Return default config
  }
  
  return all_configs.at(channel_id);
}

size_t cvedix_ba_area_loitering_node::get_channel_count() const {
  std::lock_guard<std::mutex> lock(config_mutex);
  
  return all_rois.size();
}
 
} // namespace cvedix_nodes