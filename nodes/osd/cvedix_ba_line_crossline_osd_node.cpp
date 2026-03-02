
#include "cvedix_ba_line_crossline_osd_node.h"
#include <cmath>

namespace cvedix_nodes {

// Helper function to draw arrow perpendicular to line
static void
draw_direction_arrow(cv::Mat &canvas, const cvedix_objects::cvedix_line &line,
                     const cv::Scalar &color,
                     cvedix_objects::cvedix_ba_direct_type direction) {
  // Calculate line midpoint
  int mid_x = (line.start.x + line.end.x) / 2;
  int mid_y = (line.start.y + line.end.y) / 2;

  // Calculate perpendicular direction
  double dx = line.end.x - line.start.x;
  double dy = line.end.y - line.start.y;
  double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1)
    return;

  // Normalize and get perpendicular (rotate 90 degrees)
  double perp_x = -dy / len;
  double perp_y = dx / len;

  int arrow_len = 25; // Arrow length
  int arrow_head = 8; // Arrow head size

  // Draw arrows based on direction
  if (direction == cvedix_objects::cvedix_ba_direct_type::IN ||
      direction == cvedix_objects::cvedix_ba_direct_type::BOTH) {
    // IN arrow (pointing one way)
    cv::Point arrow_start(mid_x, mid_y);
    cv::Point arrow_end(mid_x + static_cast<int>(perp_x * arrow_len),
                        mid_y + static_cast<int>(perp_y * arrow_len));
    cv::arrowedLine(canvas, arrow_start, arrow_end, color, 2, cv::LINE_AA, 0,
                    0.3);
    cv::putText(canvas, "IN", cv::Point(arrow_end.x + 5, arrow_end.y),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1);
  }

  if (direction == cvedix_objects::cvedix_ba_direct_type::OUT ||
      direction == cvedix_objects::cvedix_ba_direct_type::BOTH) {
    // OUT arrow (pointing opposite way)
    cv::Point arrow_start(mid_x, mid_y);
    cv::Point arrow_end(mid_x - static_cast<int>(perp_x * arrow_len),
                        mid_y - static_cast<int>(perp_y * arrow_len));
    cv::arrowedLine(canvas, arrow_start, arrow_end, color, 2, cv::LINE_AA, 0,
                    0.3);
    cv::putText(canvas, "OUT", cv::Point(arrow_end.x + 5, arrow_end.y),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1);
  }
}

cvedix_ba_line_crossline_osd_node::cvedix_ba_line_crossline_osd_node(
    std::string node_name, std::string font)
    : cvedix_node(node_name) {
  if (!font.empty()) {
    ft2 = cv::freetype::createFreeType2();
    ft2->loadFontData(font, 0);
  }
  this->initialized();
}

cvedix_ba_line_crossline_osd_node::~cvedix_ba_line_crossline_osd_node() {
  deinitialized();
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_line_crossline_osd_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
  // operations on osd_frame
  if (meta->osd_frame.empty()) {
    meta->osd_frame = meta->frame.clone();
  }

  auto &canvas = meta->osd_frame;

  // === Collect crossed track IDs from BA results ===
  std::set<int> crossed_ids;
  for (auto &ba_result : meta->ba_results) {
    if (ba_result->type == cvedix_objects::cvedix_ba_type::CROSSLINE) {
      for (auto &tid : ba_result->involve_target_ids_in_frame) {
        crossed_ids.insert(tid);
      }
    }
  }
  // Also accumulate into persistent set (keep crossed IDs across frames)
  for (auto &tid : crossed_ids) {
    all_crossed_track_ids.insert(tid);
  }

  // scan targets
  for (auto &i : meta->targets) {
    // track_id
    auto id = std::to_string(i->track_id);
    bool has_crossed = (i->track_id != -1 && all_crossed_track_ids.count(i->track_id) > 0);

    // Build label
    std::string labels_to_display;
    if (has_crossed) {
      // Crossed object: label_tracking_ID format
      labels_to_display = i->primary_label + "_tracking_" + id;
    } else if (i->track_id != -1) {
      labels_to_display = "#" + id + " " + i->primary_label;
    } else {
      labels_to_display = i->primary_label;
    }

    // Collect speed label separately (don't add to main label)
    std::string speed_label = "";
    bool is_violation = false;
    for (auto &label : i->secondary_labels) {
      if (label.find("km/h") != std::string::npos) {
        speed_label = label;
        if (label.find("[!]") != std::string::npos) {
          is_violation = true;
        }
      } else {
        labels_to_display += "|" + label;
      }
    }

    // Color: RED if crossed, default otherwise
    cv::Scalar bbox_color, text_color, dot_color;
    int bbox_thickness;
    if (has_crossed) {
      bbox_color = cv::Scalar(0, 0, 255);      // RED bbox
      text_color = cv::Scalar(0, 0, 255);       // RED text
      dot_color = cv::Scalar(0, 0, 255);        // RED dot
      bbox_thickness = 3;
    } else if (is_violation) {
      bbox_color = cv::Scalar(0, 0, 255);
      text_color = cv::Scalar(0, 0, 255);
      dot_color = cv::Scalar(0, 255, 255);
      bbox_thickness = 3;
    } else {
      bbox_color = cv::Scalar(255, 255, 0);     // Cyan bbox
      text_color = cv::Scalar(179, 52, 255);     // Purple text
      dot_color = cv::Scalar(0, 255, 255);       // Yellow dot
      bbox_thickness = 2;
    }

    // draw tracks if size>=2
    if (i->tracks.size() >= 2) {
      cv::Scalar trail_color = has_crossed ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 255);
      for (size_t n = 0; n < (i->tracks.size() - 1); n++) {
        auto p1 = i->tracks[n].track_point();
        auto p2 = i->tracks[n + 1].track_point();
        cv::line(canvas, cv::Point(p1.x, p1.y), cv::Point(p2.x, p2.y),
                 trail_color, 1, cv::LINE_AA);
      }
    }

    cv::rectangle(canvas, cv::Rect(i->x, i->y, i->width, i->height),
                  bbox_color, bbox_thickness);

    // Draw center dot on detected object
    int center_x = i->x + i->width / 2;
    int center_y = i->y + i->height / 2;
    cv::circle(canvas, cv::Point(center_x, center_y), 5, dot_color, cv::FILLED, cv::LINE_AA);
    cv::circle(canvas, cv::Point(center_x, center_y), 5, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);

    // Draw primary label BELOW bbox
    {
      int baseline = 0;
      auto text_size = cv::getTextSize(labels_to_display, cv::FONT_HERSHEY_SIMPLEX, 0.45, 1, &baseline);
      int text_x = i->x;
      int text_y = i->y + i->height + text_size.height + 4;

      cv::Scalar bg_color = has_crossed ? cv::Scalar(0, 0, 100) : cv::Scalar(0, 0, 0);
      cv::rectangle(canvas,
                    cv::Point(text_x - 1, i->y + i->height + 1),
                    cv::Point(text_x + text_size.width + 4, text_y + 3),
                    bg_color, cv::FILLED);

      cv::putText(canvas, labels_to_display, cv::Point(text_x + 2, text_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.45, text_color, 1, cv::LINE_AA);
    }

    // Draw speed label below primary label (with offset)
    if (!speed_label.empty()) {
      cv::Scalar speed_color = is_violation ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 200, 0);
      int baseline = 0;
      auto text_size = cv::getTextSize(speed_label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
      int text_x = i->x;
      // Offset below primary label (primary label height ~20px + gap)
      int speed_y_start = i->y + i->height + 22;
      int text_y = speed_y_start + text_size.height + 2;

      // Background rectangle
      cv::rectangle(canvas,
                    cv::Point(text_x - 1, speed_y_start),
                    cv::Point(text_x + text_size.width + 6, text_y + 3),
                    cv::Scalar(0, 0, 0), cv::FILLED);

      cv::putText(canvas, speed_label, cv::Point(text_x + 2, text_y),
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, speed_color, 2, cv::LINE_AA);
    }

    // scan sub targets
    for (auto &sub_target : i->sub_targets) {
      cv::rectangle(canvas,
                    cv::Rect(sub_target->x, sub_target->y, sub_target->width,
                             sub_target->height),
                    cv::Scalar(255));
      if (ft2 != nullptr) {
        ft2->putText(canvas, sub_target->label,
                     cv::Point(sub_target->x, sub_target->y), 20,
                     cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA, true);
      } else {
        cv::putText(canvas, sub_target->label,
                    cv::Point(sub_target->x, sub_target->y), 1, 1,
                    cv::Scalar(0, 0, 255));
      }
    }
  }

  /* Multi-line crossline draw for current channel */
  int channel = meta->channel_index;
  auto &total_crossline = all_total_crossline[channel];
  auto &line_configs = all_line_configs[channel];

  // scan ba results and update line configs from crossline events
  for (auto &ba_result : meta->ba_results) {
    if (ba_result->type == cvedix_objects::cvedix_ba_type::CROSSLINE) {
      // Extract line from ba_result
      if (ba_result->involve_region_in_frame.size() == 2) {
        cvedix_objects::cvedix_line line(ba_result->involve_region_in_frame[0],
                                         ba_result->involve_region_in_frame[1]);

        // Parse line_index from ba_label: "cross line X"
        int line_index = 0;
        std::string label = ba_result->ba_label;
        size_t pos = label.find("cross line ");
        if (pos != std::string::npos) {
          try {
            line_index = std::stoi(label.substr(pos + 11));
          } catch (...) {
            line_index = 0;
          }
        }

        // Ensure line_configs has enough entries
        while (line_configs.size() <= static_cast<size_t>(line_index)) {
          size_t idx = line_configs.size();
          cv::Scalar color = default_colors[idx % default_colors.size()];
          line_configs.push_back(
              line_display_config(cvedix_objects::cvedix_line(), color));
        }

        // Update line geometry and increment count
        line_configs[line_index].line = line;
        line_configs[line_index].crossing_count++;
        total_crossline++;
      }
    }
  }

  // Draw all lines with their individual colors
  int y_offset = 20;
  for (size_t i = 0; i < line_configs.size(); i++) {
    auto &config = line_configs[i];
    if (config.line.start.x != 0 || config.line.start.y != 0 ||
        config.line.end.x != 0 || config.line.end.y != 0) {

      // Draw line with configured color
      cv::line(canvas, cv::Point(config.line.start.x, config.line.start.y),
               cv::Point(config.line.end.x, config.line.end.y), config.color, 3,
               cv::LINE_AA);

      // Draw direction arrows (IN/OUT/BOTH)
      draw_direction_arrow(canvas, config.line, config.color, config.direction);

      // Draw line label/name at midpoint
      int mid_x = (config.line.start.x + config.line.end.x) / 2;
      int mid_y = (config.line.start.y + config.line.end.y) / 2;
      std::string line_label = config.name.empty()
                                   ? cvedix_utils::string_format("Line %zu", i)
                                   : config.name;
      cv::putText(canvas, line_label, cv::Point(mid_x - 30, mid_y - 10),
                  cv::FONT_HERSHEY_SIMPLEX, 0.5, config.color, 2);

      // Draw per-line count
      std::string count_text =
          cvedix_utils::string_format("Line %zu: %d", i, config.crossing_count);
      cv::putText(canvas, count_text, cv::Point(20, y_offset),
                  cv::FONT_HERSHEY_SIMPLEX, 0.6, config.color, 2);
      y_offset += 25;
    }
  }

  // Draw total crossline count
  cv::putText(
      canvas,
      cvedix_utils::string_format("Total crossings: %d", total_crossline),
      cv::Point(20, y_offset), cv::FONT_HERSHEY_SIMPLEX, 0.6,
      cv::Scalar(0, 0, 255), 2);

  return meta;
}

void cvedix_ba_line_crossline_osd_node::set_line_configs(
    int channel_id, const std::vector<line_display_config> &configs) {
  all_line_configs[channel_id] = configs;
  CVEDIX_INFO(cvedix_utils::string_format(
      "[%s] Set %zu line configs for channel %d", node_name.c_str(),
      configs.size(), channel_id));
}

void cvedix_ba_line_crossline_osd_node::set_line_color(int channel_id,
                                                  int line_index,
                                                  const cv::Scalar &color) {
  if (all_line_configs.count(channel_id) == 0) {
    return;
  }
  auto &configs = all_line_configs[channel_id];
  if (line_index >= 0 && line_index < static_cast<int>(configs.size())) {
    configs[line_index].color = color;
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Updated color for channel %d line %d: (%d,%d,%d)",
        node_name.c_str(), channel_id, line_index, (int)color[0], (int)color[1],
        (int)color[2]));
  }
}
} // namespace cvedix_nodes