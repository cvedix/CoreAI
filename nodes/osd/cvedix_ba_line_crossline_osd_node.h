/**
 * @file cvedix_ba_line_crossline_osd_node.h
 * @brief OSD for crossline detection behavior analysis
 *
 * Draws crosslines with customizable colors and displays crossing statistics.
 * Supports multiple lines per channel with individual colors.
 */

#pragma once

#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h" // For crossline_config
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <opencv2/freetype.hpp>
#include <vector>

namespace cvedix_nodes {

/**
 * @brief Line display configuration for OSD
 */
struct line_display_config {
  cvedix_objects::cvedix_line line;
  cv::Scalar color = cv::Scalar(0, 255, 0); // Default green
  std::string name = "";
  int crossing_count = 0;
  cvedix_objects::cvedix_ba_direct_type direction =
      cvedix_objects::cvedix_ba_direct_type::BOTH;

  line_display_config() = default;
  line_display_config(const cvedix_objects::cvedix_line &l,
                      const cv::Scalar &c = cv::Scalar(0, 255, 0),
                      const std::string &n = "",
                      cvedix_objects::cvedix_ba_direct_type dir =
                          cvedix_objects::cvedix_ba_direct_type::BOTH)
      : line(l), color(c), name(n), crossing_count(0), direction(dir) {}
};

/**
 * @brief Crossline BA visualization with multi-line color support
 */
class cvedix_ba_line_crossline_osd_node : public cvedix_node {
private:
  // support chinese font
  cv::Ptr<cv::freetype::FreeType2> ft2;

  // Multi-line support: channel_id → vector of line display configs
  std::map<int, std::vector<line_display_config>> all_line_configs;

  // Total crossings per channel (for backward compatibility display)
  std::map<int, int> all_total_crossline;

  // Default colors to cycle through for lines without explicit color
  std::vector<cv::Scalar> default_colors = {
      cv::Scalar(0, 255, 0),   // Green
      cv::Scalar(0, 0, 255),   // Red
      cv::Scalar(255, 0, 0),   // Blue
      cv::Scalar(0, 255, 255), // Yellow
      cv::Scalar(255, 0, 255), // Magenta
      cv::Scalar(255, 255, 0), // Cyan
      cv::Scalar(128, 0, 255), // Orange
      cv::Scalar(0, 128, 255)  // Gold
  };

protected:
  virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
      std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
  cvedix_ba_line_crossline_osd_node(std::string node_name, std::string font = "");
  ~cvedix_ba_line_crossline_osd_node();

  /**
   * @brief Set line configurations with colors for a channel
   * @param channel_id Target channel
   * @param configs Vector of line display configs with colors
   */
  void set_line_configs(int channel_id,
                        const std::vector<line_display_config> &configs);

  /**
   * @brief Set line color for a specific line
   * @param channel_id Target channel
   * @param line_index Line index
   * @param color New color (BGR)
   */
  void set_line_color(int channel_id, int line_index, const cv::Scalar &color);
};
} // namespace cvedix_nodes