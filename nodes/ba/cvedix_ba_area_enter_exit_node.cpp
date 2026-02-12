

#include "cvedix_ba_area_enter_exit_node.h"

namespace cvedix_nodes {

cvedix_ba_area_enter_exit_node::cvedix_ba_area_enter_exit_node(
    std::string node_name,
    std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_areas(areas),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
  
  // Initialize default configs for all areas
  for (const auto &channel_pair : all_areas) {
    int channel_id = channel_pair.first;
    size_t area_count = channel_pair.second.size();
    all_area_configs[channel_id].resize(area_count);
    // Default config: alert on both enter and exit
    for (size_t i = 0; i < area_count; i++) {
      all_area_configs[channel_id][i] = area_alert_config(true, true);
    }
  }

  CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                          to_string().c_str()));
  this->initialized();
}


cvedix_ba_area_enter_exit_node::cvedix_ba_area_enter_exit_node(
    std::string node_name,
    std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
    std::map<int, std::vector<area_alert_config>> configs,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_areas(areas),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {

  // Initialize configs from provided map; fill defaults where missing
  for (const auto &channel_pair : all_areas) {
    int channel_id = channel_pair.first;
    size_t area_count = channel_pair.second.size();

    if (configs.count(channel_id) > 0) {
      // copy provided configs but ensure sizing
      all_area_configs[channel_id] = configs.at(channel_id);
      if (all_area_configs[channel_id].size() < area_count) {
        size_t old = all_area_configs[channel_id].size();
        all_area_configs[channel_id].resize(area_count);
        for (size_t i = old; i < area_count; ++i) {
          all_area_configs[channel_id][i] = area_alert_config(true, true);
        }
      }
    } else {
      all_area_configs[channel_id].resize(area_count);
      for (size_t i = 0; i < area_count; i++) {
        all_area_configs[channel_id][i] = area_alert_config(true, true);
      }
    }
  }

  CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                          to_string().c_str()));
  this->initialized();
}

cvedix_ba_area_enter_exit_node::~cvedix_ba_area_enter_exit_node() {
  deinitialized();
}

std::string cvedix_ba_area_enter_exit_node::to_string() {
  std::lock_guard<std::mutex> lock(areas_mutex);
  /*
   * return all areas for all channels
   * [channel 0: area0(polygon points) area1(polygon points)][channel 1: ...]
   */
  std::stringstream ss;
  for (const auto &channel_pair : all_areas) {
    ss << "[channel" << channel_pair.first << ": ";
    int area_idx = 0;
    for (const auto &polygon : channel_pair.second) {
      ss << "area" << area_idx << "(polygon:" << polygon.size() << " pts) ";
      area_idx++;
    }
    ss << "]";
  }
  return ss.str();
}

bool cvedix_ba_area_enter_exit_node::is_inside_polygon(
    const cvedix_objects::cvedix_point &p,
    const std::vector<cvedix_objects::cvedix_point> &polygon) {
  // Ray-casting algorithm for point-in-polygon test
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

std::set<int> cvedix_ba_area_enter_exit_node::get_areas_containing_point(
    const cvedix_objects::cvedix_point &p,
    const std::vector<std::vector<cvedix_objects::cvedix_point>> &areas) {
  std::set<int> result;
  for (size_t i = 0; i < areas.size(); i++) {
    if (is_inside_polygon(p, areas[i])) {
      result.insert(i);
    }
  }
  return result;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_area_enter_exit_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  // Check if channel has any areas configured
  if (all_areas.count(meta->channel_index) == 0 ||
      all_areas[meta->channel_index].empty()) {
    return meta;
  }

  // Get all areas for current channel
  const auto &channel_areas = all_areas[meta->channel_index];
  auto &channel_configs = all_area_configs[meta->channel_index];
  auto &channel_previous_status = all_previous_area_status[meta->channel_index];

  // For each tracked target
  for (auto &target : meta->targets) {
    auto len = target->tracks.size();
    if (len > 1 && target->track_id >= 0) {
      // Get current and previous track positions
      auto current_point = target->tracks[len - 1].track_point();
      auto previous_point = target->tracks[len - 2].track_point();

      // Get which areas contain current and previous positions
      auto current_areas = get_areas_containing_point(current_point, channel_areas);
      auto previous_areas = channel_previous_status[target->track_id];

      // Detect ENTER events: areas in current but not in previous
      for (int area_index : current_areas) {
        if (previous_areas.find(area_index) == previous_areas.end()) {
          // Object entered this area
          const area_alert_config &config = 
              (area_index < channel_configs.size()) 
                  ? channel_configs[area_index] 
                  : area_alert_config();

          // Check if enter alert is enabled
          if (config.alert_on_enter) {
            std::vector<int> involve_targets = {target->track_id};

            // Recording filenames
            std::string image_file_name_without_ext = "";
            std::string video_file_name_without_ext = "";

            // Send image record control meta
            if (need_record_image) {
              image_file_name_without_ext = cvedix_utils::time_format(
                  NOW, "area_enter_ch" + std::to_string(meta->channel_index) +
                           "_area" + std::to_string(area_index) +
                           "__<year><mon><day><hour><min><sec><mili>");
              auto image_record_control_meta = std::make_shared<
                  cvedix_objects::cvedix_image_record_control_meta>(
                  meta->channel_index, image_file_name_without_ext, true);
              pendding_meta(image_record_control_meta);
            }

            // Send video record control meta
            if (need_record_video) {
              video_file_name_without_ext = cvedix_utils::time_format(
                  NOW, "area_enter_ch" + std::to_string(meta->channel_index) +
                           "_area" + std::to_string(area_index) +
                           "__<year><mon><day><hour><min><sec><mili>");
              auto video_record_control_meta = std::make_shared<
                  cvedix_objects::cvedix_video_record_control_meta>(
                  meta->channel_index, video_file_name_without_ext);
              pendding_meta(video_record_control_meta);
            }

            // Create BA result with area polygon as region
            const auto &area = channel_areas[area_index];
            std::vector<cvedix_objects::cvedix_point> involve_region = area;

            std::string label = "enter area " + std::to_string(area_index);
            if (!config.name.empty()) {
              label += " (" + config.name + ")";
            }

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::AREA_ENTER,
                meta->channel_index, meta->frame_index, involve_targets,
                involve_region, label, image_file_name_without_ext,
                video_file_name_without_ext);

            meta->ba_results.push_back(ba_result);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [channel %d] [area %d] target %d entered", 
                node_name.c_str(), meta->channel_index, area_index,
                target->track_id));
          }
        }
      }

      // Detect EXIT events: areas in previous but not in current
      for (int area_index : previous_areas) {
        if (current_areas.find(area_index) == current_areas.end()) {
          // Object exited this area
          const area_alert_config &config = 
              (area_index < channel_configs.size()) 
                  ? channel_configs[area_index] 
                  : area_alert_config();

          // Check if exit alert is enabled
          if (config.alert_on_exit) {
            std::vector<int> involve_targets = {target->track_id};

            // Recording filenames
            std::string image_file_name_without_ext = "";
            std::string video_file_name_without_ext = "";

            // Send image record control meta
            if (need_record_image) {
              image_file_name_without_ext = cvedix_utils::time_format(
                  NOW, "area_exit_ch" + std::to_string(meta->channel_index) +
                           "_area" + std::to_string(area_index) +
                           "__<year><mon><day><hour><min><sec><mili>");
              auto image_record_control_meta = std::make_shared<
                  cvedix_objects::cvedix_image_record_control_meta>(
                  meta->channel_index, image_file_name_without_ext, true);
              pendding_meta(image_record_control_meta);
            }

            // Send video record control meta
            if (need_record_video) {
              video_file_name_without_ext = cvedix_utils::time_format(
                  NOW, "area_exit_ch" + std::to_string(meta->channel_index) +
                           "_area" + std::to_string(area_index) +
                           "__<year><mon><day><hour><min><sec><mili>");
              auto video_record_control_meta = std::make_shared<
                  cvedix_objects::cvedix_video_record_control_meta>(
                  meta->channel_index, video_file_name_without_ext);
              pendding_meta(video_record_control_meta);
            }

            // Create BA result with area polygon as region
            const auto &area = channel_areas[area_index];
            std::vector<cvedix_objects::cvedix_point> involve_region = area;

            std::string label = "exit area " + std::to_string(area_index);
            if (!config.name.empty()) {
              label += " (" + config.name + ")";
            }

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::AREA_EXIT, meta->channel_index,
                meta->frame_index, involve_targets, involve_region, label,
                image_file_name_without_ext, video_file_name_without_ext);

            meta->ba_results.push_back(ba_result);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [channel %d] [area %d] target %d exited", 
                node_name.c_str(), meta->channel_index, area_index,
                target->track_id));
          }
        }
      }

      // Update previous status for this target
      channel_previous_status[target->track_id] = current_areas;
      // Update last seen frame for this track
      all_previous_last_seen_frame[meta->channel_index][target->track_id] = meta->frame_index;
    }
  }

  // Update last-seen for tracks present in this frame (ensure they are fresh)
  for (auto &t : meta->targets) {
    if (t->track_id >= 0)
      all_previous_last_seen_frame[meta->channel_index][t->track_id] = meta->frame_index;
  }

  // Prune previous status entries for track ids that have been inactive
  // longer than the configured timeout (in seconds). Convert timeout to
  // frames using meta->fps when available; fallback to 25 FPS.
  int fps = (meta->fps > 0) ? meta->fps : 25;
  int frames_to_keep = fps * inactive_timeout_seconds;

  for (auto it = channel_previous_status.begin(); it != channel_previous_status.end();) {
    int track_id = it->first;
    int last_seen = 0;
    if (all_previous_last_seen_frame[meta->channel_index].count(track_id) > 0) {
      last_seen = all_previous_last_seen_frame[meta->channel_index][track_id];
    }

    if (meta->frame_index - last_seen > frames_to_keep) {
      // remove both previous area status and last-seen entry
      all_previous_last_seen_frame[meta->channel_index].erase(track_id);
      // CVEDIX_INFO(cvedix_utils::string_format(
      //     "[%s] [channel %d] Pruning inactive track %d from area status",
      //     node_name.c_str(), meta->channel_index, track_id));
      it = channel_previous_status.erase(it);
    } else {
      ++it;
    }
  }

  return meta;
}

bool cvedix_ba_area_enter_exit_node::set_areas(
    const std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> &areas) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  all_areas = areas;
  all_previous_area_status.clear();

  // Reset configs to defaults
  all_area_configs.clear();
  for (const auto &channel_pair : all_areas) {
    int channel_id = channel_pair.first;
    size_t area_count = channel_pair.second.size();
    all_area_configs[channel_id].resize(area_count);
    for (size_t i = 0; i < area_count; i++) {
      all_area_configs[channel_id][i] = area_alert_config(true, true);
    }
  }

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Area config replaced at runtime: %s", node_name.c_str(),
      to_string().c_str()));

  return true;
}

int cvedix_ba_area_enter_exit_node::add_area(
    int channel_id, const std::vector<cvedix_objects::cvedix_point> &area) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  all_areas[channel_id].push_back(area);
  int area_index = all_areas[channel_id].size() - 1;
  
  // Add default config
  all_area_configs[channel_id].push_back(area_alert_config(true, true));

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Added area %d to channel %d: polygon with %zu points", node_name.c_str(),
      area_index, channel_id, area.size()));

  return area_index;
}

int cvedix_ba_area_enter_exit_node::add_area(
    int channel_id, const std::vector<cvedix_objects::cvedix_point> &area,
    const area_alert_config &config) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  all_areas[channel_id].push_back(area);
  int area_index = all_areas[channel_id].size() - 1;
  
  // Add provided config
  all_area_configs[channel_id].push_back(config);

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Added area %d to channel %d: polygon with %zu points, config [enter:%d, "
      "exit:%d]",
      node_name.c_str(), area_index, channel_id, area.size(),
      config.alert_on_enter, config.alert_on_exit));

  return area_index;
}

void cvedix_ba_area_enter_exit_node::clear_areas() {
  std::lock_guard<std::mutex> lock(areas_mutex);

  all_areas.clear();
  all_area_configs.clear();
  all_previous_area_status.clear();

  CVEDIX_INFO(cvedix_utils::string_format("[%s] Cleared all areas",
                                          node_name.c_str()));
}

bool cvedix_ba_area_enter_exit_node::remove_channel_areas(int channel_id) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  if (all_areas.count(channel_id) == 0) {
    return false;
  }

  all_areas.erase(channel_id);
  all_area_configs.erase(channel_id);
  all_previous_area_status.erase(channel_id);

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Removed all areas for channel %d", node_name.c_str(), channel_id));

  return true;
}

bool cvedix_ba_area_enter_exit_node::remove_area(int channel_id,
                                                  int area_index) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  if (all_areas.count(channel_id) == 0 ||
      area_index < 0 ||
      area_index >= all_areas[channel_id].size()) {
    return false;
  }

  all_areas[channel_id].erase(all_areas[channel_id].begin() + area_index);
  
  if (area_index < all_area_configs[channel_id].size()) {
    all_area_configs[channel_id].erase(all_area_configs[channel_id].begin() +
                                        area_index);
  }

  // Clear previous status to avoid stale area indices
  all_previous_area_status[channel_id].clear();

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Removed area %d from channel %d", node_name.c_str(), area_index,
      channel_id));

  return true;
}

size_t cvedix_ba_area_enter_exit_node::get_area_count(int channel_id) const {
  if (all_areas.count(channel_id) == 0) {
    return 0;
  }
  return all_areas.at(channel_id).size();
}

bool cvedix_ba_area_enter_exit_node::set_area_config(
    int channel_id, int area_index, const area_alert_config &config) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  if (all_areas.count(channel_id) == 0 ||
      area_index < 0 ||
      area_index >= all_areas[channel_id].size()) {
    return false;
  }

  // Ensure config vector is properly sized
  if (all_area_configs[channel_id].size() <= area_index) {
    all_area_configs[channel_id].resize(area_index + 1);
  }

  all_area_configs[channel_id][area_index] = config;

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Updated config for channel %d area %d [enter:%d, exit:%d]",
      node_name.c_str(), channel_id, area_index, config.alert_on_enter,
      config.alert_on_exit));

  return true;
}

area_alert_config
cvedix_ba_area_enter_exit_node::get_area_config(int channel_id,
                                                 int area_index) const {
  if (all_area_configs.count(channel_id) == 0 ||
      area_index < 0 ||
      area_index >= all_area_configs.at(channel_id).size()) {
    return area_alert_config(); // Return default config
  }

  return all_area_configs.at(channel_id)[area_index];
}

std::vector<area_alert_config>
cvedix_ba_area_enter_exit_node::get_all_configs(int channel_id) const {
  if (all_area_configs.count(channel_id) == 0) {
    return std::vector<area_alert_config>();
  }

  return all_area_configs.at(channel_id);
}

} // namespace cvedix_nodes
