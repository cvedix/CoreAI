
#include "cvedix_ba_movement_node.h"

namespace cvedix_nodes {

cvedix_ba_movement_node::cvedix_ba_movement_node(
    std::string node_name,
    std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
    std::map<int, std::vector<movement_alert_config>> configs,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_areas(areas),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
  // Initialize configs from provided map; fill defaults where missing
  for (const auto &channel_pair : all_areas) {
    int channel_id = channel_pair.first;
    size_t area_count = channel_pair.second.size();
    if (configs.count(channel_id) > 0) {
      all_area_configs[channel_id] = configs.at(channel_id);
      if (all_area_configs[channel_id].size() < area_count) {
        size_t old = all_area_configs[channel_id].size();
        all_area_configs[channel_id].resize(area_count);
        for (size_t i = old; i < area_count; ++i) {
          // Default config: alert enabled, relevant_classes empty (monitor all)
          all_area_configs[channel_id][i] = movement_alert_config(true, "", cv::Scalar(255, 0, 0), cvedix_objects::cvedix_rect_anchor_point::CENTER, {});
        }
      }
    } else {
      all_area_configs[channel_id].resize(area_count);
      for (size_t i = 0; i < area_count; i++) {
        // Default config: alert enabled, relevant_classes empty (monitor all)
        all_area_configs[channel_id][i] = movement_alert_config(true, "", cv::Scalar(255, 0, 0), cvedix_objects::cvedix_rect_anchor_point::CENTER, {});
      }
    }
  }
  this->initialized();
}

cvedix_ba_movement_node::~cvedix_ba_movement_node() {
  deinitialized();
}

std::string cvedix_ba_movement_node::to_string() {
  std::lock_guard<std::mutex> lock(areas_mutex);
  std::stringstream ss;
  for (const auto &channel_pair : all_areas) {
    ss << "[channel " << channel_pair.first << ": ";
    for (size_t i = 0; i < channel_pair.second.size(); ++i) {
      ss << "area" << i << "(" << all_area_configs.at(channel_pair.first)[i].name << ") ";
    }
    ss << "] ";
  }
  return ss.str();
}

bool cvedix_ba_movement_node::is_inside_polygon(const cvedix_objects::cvedix_point &p,
                        const std::vector<cvedix_objects::cvedix_point> &polygon) {
  // Ray-casting algorithm (same as area_enter_exit_node)
  int n = polygon.size();
  int cnt = 0;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
        (p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x))
      cnt++;
  }
  return cnt % 2 == 1;
}

std::set<int> cvedix_ba_movement_node::get_areas_containing_point(
      const cvedix_objects::cvedix_rect &bbox,
      const std::vector<std::vector<cvedix_objects::cvedix_point>> &areas,
      const std::vector<movement_alert_config> &configs,
      const std::string &object_class) {
  std::set<int> indices;
  for (size_t i = 0; i < areas.size(); ++i) {
    cvedix_objects::cvedix_point anchor = bbox.track_point(configs[i].anchor_point);
    if (configs[i].relevant_classes.count(object_class) && is_inside_polygon(anchor, areas[i])) {
      indices.insert(i);
    }
  }
  return indices;
}

std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ba_movement_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
  std::lock_guard<std::mutex> lock(areas_mutex);

  // Check if channel has any areas configured
  if (all_areas.count(meta->channel_index) == 0 || all_areas[meta->channel_index].empty()) {
    return meta;
  }

  // Get all areas and configs for current channel
  const auto &channel_areas = all_areas[meta->channel_index];
  const auto &channel_configs = all_area_configs[meta->channel_index];

  // For each detected target
  for (const auto &target : meta->targets) {
    // Get the object's class label
    const std::string &label = target->primary_label;

    // For each area, check if this object's class is relevant
    for (size_t area_index = 0; area_index < channel_areas.size(); ++area_index) {
      const auto &config = channel_configs[area_index];
      // If user did not configure relevant_classes, match all
      bool match = config.relevant_classes.empty() || config.relevant_classes.count(label) > 0;
      if (!match) continue;

      // Check if the object's center is inside the area
      auto bbox = target->get_rect();
      auto track_point = bbox.track_point(config.anchor_point);
      if (is_inside_polygon(track_point, channel_areas[area_index])) {
        // Check if movement alert is enabled
        if (config.alert_on_movement) {
          std::vector<int> involve_targets = {target->track_id};

          // Recording filenames
          std::string image_file_name_without_ext = "";
          std::string video_file_name_without_ext = "";

            // Send image record control meta
            if (need_record_image) {
                image_file_name_without_ext = cvedix_utils::time_format(
                    NOW, "movement_ch" + std::to_string(meta->channel_index) +
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
                  NOW, "movement_ch" + std::to_string(meta->channel_index) +
                           "_area" + std::to_string(area_index) +
                           "__<year><mon><day><hour><min><sec><mili>");
              auto video_record_control_meta = std::make_shared<
                  cvedix_objects::cvedix_video_record_control_meta>(
                  meta->channel_index, video_file_name_without_ext);
              pendding_meta(video_record_control_meta);
            }

          // Trigger movement event (add BA result)
          std::vector<cvedix_objects::cvedix_point> involve_region = channel_areas[area_index];
          std::string event_label = "movement in area " + std::to_string(area_index);
          if (!config.name.empty()) {
            event_label += " (" + config.name + ")";
          }
          auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
              cvedix_objects::cvedix_ba_type::AREA_ENTER,
              meta->channel_index, meta->frame_index, involve_targets,
              involve_region, event_label, image_file_name_without_ext, video_file_name_without_ext);
          meta->ba_results.push_back(ba_result);
        }
      }
    }
  }
  return meta;
}

bool cvedix_ba_movement_node::set_areas(const std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> &areas) {
  std::lock_guard<std::mutex> lock(areas_mutex);
  all_areas = areas;
  return true;
}

int cvedix_ba_movement_node::add_area(int channel_id, const std::vector<cvedix_objects::cvedix_point> &area) {
  std::lock_guard<std::mutex> lock(areas_mutex);
  all_areas[channel_id].push_back(area);
  all_area_configs[channel_id].push_back(movement_alert_config());
  return all_areas[channel_id].size() - 1;
}

int cvedix_ba_movement_node::add_area(int channel_id, const std::vector<cvedix_objects::cvedix_point> &area,
               const movement_alert_config &config) {
  std::lock_guard<std::mutex> lock(areas_mutex);
  all_areas[channel_id].push_back(area);
  all_area_configs[channel_id].push_back(config);
  return all_areas[channel_id].size() - 1;
}

void cvedix_ba_movement_node::clear_areas() {
  std::lock_guard<std::mutex> lock(areas_mutex);
  all_areas.clear();
  all_area_configs.clear();
}

bool cvedix_ba_movement_node::remove_channel_areas(int channel_id) {
  std::lock_guard<std::mutex> lock(areas_mutex);
  bool existed = all_areas.count(channel_id) > 0;
  all_areas.erase(channel_id);
  all_area_configs.erase(channel_id);
  return existed;
}

bool cvedix_ba_movement_node::remove_area(int channel_id, int area_index) {
  std::lock_guard<std::mutex> lock(areas_mutex);
  if (all_areas.count(channel_id) == 0 || area_index >= all_areas[channel_id].size()) return false;
  all_areas[channel_id].erase(all_areas[channel_id].begin() + area_index);
  all_area_configs[channel_id].erase(all_area_configs[channel_id].begin() + area_index);
  return true;
}

size_t cvedix_ba_movement_node::get_area_count(int channel_id) const {
  if (all_areas.count(channel_id) == 0) return 0;
  return all_areas.at(channel_id).size();
}

bool cvedix_ba_movement_node::set_area_config(int channel_id, int area_index,
                      const movement_alert_config &config) {
  std::lock_guard<std::mutex> lock(areas_mutex);
  if (all_area_configs.count(channel_id) == 0 || area_index >= all_area_configs[channel_id].size()) return false;
  all_area_configs[channel_id][area_index] = config;
  return true;
}

movement_alert_config cvedix_ba_movement_node::get_area_config(int channel_id, int area_index) const {
  if (all_area_configs.count(channel_id) == 0 || area_index >= all_area_configs.at(channel_id).size()) return movement_alert_config();
  return all_area_configs.at(channel_id)[area_index];
}

std::vector<movement_alert_config> cvedix_ba_movement_node::get_all_configs(int channel_id) const {
  if (all_area_configs.count(channel_id) == 0) return {};
  return all_area_configs.at(channel_id);
}

} // namespace cvedix_nodes
