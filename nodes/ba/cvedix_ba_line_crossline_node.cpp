#include "cvedix_ba_line_crossline_node.h"

namespace cvedix_nodes {

// Multi-line constructor (new)
cvedix_ba_line_crossline_node::cvedix_ba_line_crossline_node(
    std::string node_name,
    std::map<int, std::vector<cvedix_objects::cvedix_line>> lines,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), all_lines(lines),
      need_record_image(need_record_image),
      need_record_video(need_record_video),
      include_target_crops(false) {
  CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                          to_string().c_str()));
  this->initialized();
}

// Single-line constructor (backward compatible)
cvedix_ba_line_crossline_node::cvedix_ba_line_crossline_node(
    std::string node_name, std::map<int, cvedix_objects::cvedix_line> lines,
    bool need_record_image, bool need_record_video)
    : cvedix_node(node_name), need_record_image(need_record_image),
      need_record_video(need_record_video),
      include_target_crops(false) {
  // Convert single-line map to multi-line map
  for (const auto &p : lines) {
    all_lines[p.first] = {p.second};
  }
  CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                          to_string().c_str()));
  this->initialized();
}

cvedix_ba_line_crossline_node::~cvedix_ba_line_crossline_node() { deinitialized(); }

std::string cvedix_ba_line_crossline_node::to_string() {
  std::lock_guard<std::mutex> lock(lines_mutex);
  /*
   * return all lines for all channels
   * [channel 0: line0(x1,y1-x2,y2) line1(x1,y1-x2,y2)][channel 1: ...]
   */
  std::stringstream ss;
  for (const auto &channel_pair : all_lines) {
    ss << "[channel" << channel_pair.first << ": ";
    int line_idx = 0;
    for (const auto &line : channel_pair.second) {
      ss << "line" << line_idx << "(" << line.start.x << "," << line.start.y
         << "-" << line.end.x << "," << line.end.y << ") ";
      line_idx++;
    }
    ss << "]";
  }
  return ss.str();
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_line_crossline_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  // Check if channel has any lines configured
  if (all_lines.count(meta->channel_index) == 0 ||
      all_lines[meta->channel_index].empty()) {
    return meta;
  }

  // Get all lines for current channel
  const auto &channel_lines = all_lines[meta->channel_index];
  auto &channel_counters = all_total_crossline[meta->channel_index];

  // For each tracked target
  for (auto &target : meta->targets) {
    auto len = target->tracks.size();
    if (len > 1 && target->track_id >= 0) {
      // Check the last 2 points in tracks
      auto p1 = target->tracks[len - 1].track_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
      auto p2 = target->tracks[len - 2].track_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

      // Check crossing for EACH line
      for (size_t line_index = 0; line_index < channel_lines.size();
           line_index++) {
        const auto &line = channel_lines[line_index];

        auto check1 = at_1_side_of_line(p1, line);
        auto check2 = at_1_side_of_line(p2, line);

        // XOR: target crossed line in this frame
        if (check1 ^ check2) {
          // Determine crossing direction
          // check2=true (was on positive side) -> check1=false (now on
          // negative) = IN check2=false (was on negative side) -> check1=true
          // (now on positive) = OUT
          cvedix_objects::cvedix_ba_direct_type detected_direction =
              (check2 && !check1) ? cvedix_objects::cvedix_ba_direct_type::IN
                                  : cvedix_objects::cvedix_ba_direct_type::OUT;

          // Get config to check direction filter
          cvedix_objects::cvedix_ba_direct_type required_direction =
              cvedix_objects::cvedix_ba_direct_type::BOTH;
          if (all_configs.count(meta->channel_index) > 0 &&
              line_index < all_configs.at(meta->channel_index).size()) {
            required_direction =
                all_configs.at(meta->channel_index)[line_index].direction;
          }

          // Skip if direction doesn't match (unless BOTH)
          if (required_direction !=
                  cvedix_objects::cvedix_ba_direct_type::BOTH &&
              required_direction != detected_direction) {
            continue; // Direction doesn't match filter, skip this crossing
          }

          // Update counter for this specific line
          channel_counters[line_index]++;
          int total_count = channel_counters[line_index];

          std::vector<int> involve_targets = {target->track_id};

          // Recording filenames
          std::string image_file_name_without_ext = "";
          std::string video_file_name_without_ext = "";

          // Direction label suffix
          std::string dir_label =
              (detected_direction == cvedix_objects::cvedix_ba_direct_type::IN)
                  ? " [IN]"
                  : " [OUT]";

          // Send image record control meta
          if (need_record_image) {
            image_file_name_without_ext = cvedix_utils::time_format(
                NOW, "crossline_ch" + std::to_string(meta->channel_index) +
                         "_line" + std::to_string(line_index) +
                         "__<year><mon><day><hour><min><sec><mili>");
            auto image_record_control_meta = std::make_shared<
                cvedix_objects::cvedix_image_record_control_meta>(
                meta->channel_index, image_file_name_without_ext, true);
            pendding_meta(image_record_control_meta);
          }

          // Send video record control meta
          if (need_record_video) {
            video_file_name_without_ext = cvedix_utils::time_format(
                NOW, "crossline_ch" + std::to_string(meta->channel_index) +
                         "_line" + std::to_string(line_index) +
                         "__<year><mon><day><hour><min><sec><mili>");
            auto video_record_control_meta = std::make_shared<
                cvedix_objects::cvedix_video_record_control_meta>(
                meta->channel_index, video_file_name_without_ext);
            pendding_meta(video_record_control_meta);
          }

          // Create BA result with line_index and direction in label
          std::vector<cvedix_objects::cvedix_point> involve_region{line.start,
                                                                   line.end};
          std::string label =
              "cross line " + std::to_string(line_index) + dir_label;

          auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
              cvedix_objects::cvedix_ba_type::CROSSLINE, meta->channel_index,
              meta->frame_index, involve_targets, involve_region, label,
              image_file_name_without_ext, video_file_name_without_ext);

          // Populate enhanced fields
          ba_result->stamp_now();
          ba_result->region_type = "line";
          ba_result->region_index = static_cast<int>(line_index);
          // Use config name if available
          if (all_configs.count(meta->channel_index) > 0 &&
              line_index < all_configs.at(meta->channel_index).size()) {
            ba_result->region_name = all_configs.at(meta->channel_index)[line_index].name;
          }
          ba_result->populate_target_details(meta->targets, meta->frame, include_target_crops);

          meta->ba_results.push_back(ba_result);

          CVEDIX_INFO(cvedix_utils::string_format(
              "[%s] [channel %d] [line %zu] target crossed%s, total for this "
              "line: [%d]",
              node_name.c_str(), meta->channel_index, line_index,
              dir_label.c_str(), total_count));
        }
      }
    }
  }

  return meta;
}

bool cvedix_ba_line_crossline_node::at_1_side_of_line(
    cvedix_objects::cvedix_point p, cvedix_objects::cvedix_line line) {
  auto p1 = line.start;
  auto p2 = line.end;

  if (p1.x == p2.x) {
    return p.x < p1.x;
  }

  if (p1.y == p2.y) {
    return p.y < p1.y;
  }

  if (p2.x < p1.x) {
    auto tmp = p2;
    p2 = p1;
    p1 = tmp;
  }

  int ret = (p2.y - p.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p2.x - p.x);
  return ret < 0;
}

// Multi-line set_lines
bool cvedix_ba_line_crossline_node::set_lines(
    const std::map<int, std::vector<cvedix_objects::cvedix_line>> &lines) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  all_lines = lines;
  all_total_crossline.clear();

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Crossline config replaced at runtime: %s", node_name.c_str(),
      to_string().c_str()));

  return true;
}

// Single-line set_lines (backward compatible)
bool cvedix_ba_line_crossline_node::set_lines(
    const std::map<int, cvedix_objects::cvedix_line> &lines) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  all_lines.clear();
  for (const auto &p : lines) {
    all_lines[p.first] = {p.second};
  }
  all_total_crossline.clear();

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Crossline config replaced at runtime: %s", node_name.c_str(),
      to_string().c_str()));

  return true;
}

int cvedix_ba_line_crossline_node::add_line(
    int channel_id, const cvedix_objects::cvedix_line &line) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  all_lines[channel_id].push_back(line);
  int line_index = all_lines[channel_id].size() - 1;
  all_total_crossline[channel_id][line_index] = 0;

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Added line %d to channel %d: (%d,%d)-(%d,%d)", node_name.c_str(),
      line_index, channel_id, line.start.x, line.start.y, line.end.x,
      line.end.y));

  return line_index;
}

void cvedix_ba_line_crossline_node::clear_lines() {
  std::lock_guard<std::mutex> lock(lines_mutex);

  all_lines.clear();
  all_total_crossline.clear();

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] All crosslines cleared at runtime", node_name.c_str()));
}

bool cvedix_ba_line_crossline_node::remove_channel_lines(int channel_id) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  bool existed = all_lines.erase(channel_id) > 0;
  all_total_crossline.erase(channel_id);

  if (existed) {
    CVEDIX_INFO(
        cvedix_utils::string_format("[%s] All lines removed for channel %d",
                                    node_name.c_str(), channel_id));
  }

  return existed;
}

bool cvedix_ba_line_crossline_node::remove_line(int channel_id, int line_index) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  if (all_lines.count(channel_id) == 0) {
    return false;
  }

  auto &lines = all_lines[channel_id];
  if (line_index < 0 || line_index >= static_cast<int>(lines.size())) {
    return false;
  }

  lines.erase(lines.begin() + line_index);

  // Rebuild counters for this channel (indices shifted)
  all_total_crossline[channel_id].clear();

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Removed line %d from channel %d, remaining lines: %zu",
      node_name.c_str(), line_index, channel_id, lines.size()));

  return true;
}

size_t cvedix_ba_line_crossline_node::get_line_count(int channel_id) const {
  if (all_lines.count(channel_id) == 0) {
    return 0;
  }
  return all_lines.at(channel_id).size();
}

int cvedix_ba_line_crossline_node::get_crossline_count(int channel_id,
                                                  int line_index) const {
  if (all_total_crossline.count(channel_id) == 0) {
    return 0;
  }
  const auto &channel_counters = all_total_crossline.at(channel_id);
  if (channel_counters.count(line_index) == 0) {
    return 0;
  }
  return channel_counters.at(line_index);
}

// ========== New Color Configuration APIs ==========

int cvedix_ba_line_crossline_node::add_line(int channel_id,
                                       const crossline_config &config) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  all_lines[channel_id].push_back(config.line);
  all_configs[channel_id].push_back(config);
  int line_index = all_lines[channel_id].size() - 1;
  all_total_crossline[channel_id][line_index] = 0;

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Added line %d to channel %d: (%d,%d)-(%d,%d) color=(%d,%d,%d) "
      "name='%s'",
      node_name.c_str(), line_index, channel_id, config.line.start.x,
      config.line.start.y, config.line.end.x, config.line.end.y,
      (int)config.color[0], (int)config.color[1], (int)config.color[2],
      config.name.c_str()));

  return line_index;
}

bool cvedix_ba_line_crossline_node::set_line_color(int channel_id, int line_index,
                                              const cv::Scalar &color) {
  std::lock_guard<std::mutex> lock(lines_mutex);

  if (all_configs.count(channel_id) == 0) {
    return false;
  }
  auto &configs = all_configs[channel_id];
  if (line_index < 0 || line_index >= static_cast<int>(configs.size())) {
    return false;
  }

  configs[line_index].color = color;

  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Updated color for channel %d line %d: (%d,%d,%d)",
      node_name.c_str(), channel_id, line_index, (int)color[0], (int)color[1],
      (int)color[2]));

  return true;
}

crossline_config
cvedix_ba_line_crossline_node::get_line_config(int channel_id,
                                          int line_index) const {
  if (all_configs.count(channel_id) == 0) {
    return crossline_config();
  }
  const auto &configs = all_configs.at(channel_id);
  if (line_index < 0 || line_index >= static_cast<int>(configs.size())) {
    return crossline_config();
  }
  return configs[line_index];
}

std::vector<crossline_config>
cvedix_ba_line_crossline_node::get_all_configs(int channel_id) const {
  if (all_configs.count(channel_id) == 0) {
    return {};
  }
  return all_configs.at(channel_id);
}

} // namespace cvedix_nodes