/**
 * @file cvedix_ba_crossline_node.h
 * @brief Crossline detection behavior analysis node
 *
 * This node detects when tracked objects cross defined lines in the video
 * frame. Commonly used for:
 * - Counting vehicles/pedestrians crossing a boundary
 * - Intrusion detection
 * - Traffic monitoring
 *
 * @section crossline_overview How It Works
 * 1. Receives tracked targets from upstream tracker node
 * 2. Compares each object's current and previous positions
 * 3. Detects line crossing events
 * 4. Optionally triggers image/video recording
 *
 * @section crossline_multichannel Multi-Channel & Multi-Line Support
 * Each channel can have MULTIPLE detection lines.
 * Channels without configured lines skip crossline detection.
 * Each crossing event includes line_index for differentiation.
 *
 * @section crossline_usage Usage Example
 * @code
 * // Multiple lines per channel
 * std::map<int, std::vector<cvedix_line>> lines = {
 *     {0, {
 *         cvedix_line(cvedix_point(100, 300), cvedix_point(500, 300)),  // line
 * 0: entrance cvedix_line(cvedix_point(100, 500), cvedix_point(500, 500))   //
 * line 1: exit
 *     }}
 * };
 * auto crossline = std::make_shared<cvedix_ba_crossline_node>(
 *     "crossline",
 *     lines,
 *     true,   // record image on crossing
 *     false   // don't record video
 * );
 * crossline->attach_to({tracker_node});
 * @endcode
 *
 * @see cvedix_ba_jam_node Traffic jam detection
 * @see cvedix_ba_stop_node Stop detection
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <opencv2/core.hpp>
#include <vector>

namespace cvedix_nodes {

/**
 * @brief Configuration for a single crossline with color and direction
 * customization
 */
struct crossline_config {
  /// @brief The detection line geometry
  cvedix_objects::cvedix_line line;

  /// @brief Line color in BGR format (default: green)
  cv::Scalar color = cv::Scalar(0, 255, 0);

  /// @brief Optional name/label for this line (e.g., "entrance", "exit")
  std::string name = "";

  /// @brief Detection direction: IN, OUT, or BOTH (default: BOTH)
  cvedix_objects::cvedix_ba_direct_type direction =
      cvedix_objects::cvedix_ba_direct_type::BOTH;

  /// @brief Default constructor
  crossline_config() = default;

  /// @brief Constructor with line only (default green color, BOTH directions)
  crossline_config(const cvedix_objects::cvedix_line &l)
      : line(l), color(cv::Scalar(0, 255, 0)), name(""),
        direction(cvedix_objects::cvedix_ba_direct_type::BOTH) {}

  /// @brief Constructor with line and color
  crossline_config(const cvedix_objects::cvedix_line &l, const cv::Scalar &c)
      : line(l), color(c), name(""),
        direction(cvedix_objects::cvedix_ba_direct_type::BOTH) {}

  /// @brief Constructor with line, color, and name
  crossline_config(const cvedix_objects::cvedix_line &l, const cv::Scalar &c,
                   const std::string &n)
      : line(l), color(c), name(n),
        direction(cvedix_objects::cvedix_ba_direct_type::BOTH) {}

  /// @brief Full constructor with line, color, name, and direction
  crossline_config(const cvedix_objects::cvedix_line &l, const cv::Scalar &c,
                   const std::string &n,
                   cvedix_objects::cvedix_ba_direct_type dir)
      : line(l), color(c), name(n), direction(dir) {}
};

/**
 * @brief Crossline detection behavior analysis node
 *
 * Detects when tracked objects cross user-defined lines in video frames.
 * Supports multiple channels with multiple detection lines per channel.
 *
 * @note Requires tracked objects (must be attached after a tracker node)
 *
 * @see cvedix_node Base class
 */
class cvedix_ba_crossline_node : public cvedix_node {
private:
  /// @brief Crossline counters per channel per line: channel_id → line_index →
  /// count
  std::map<int, std::map<int, int>> all_total_crossline;

  /// @brief Detection lines per channel (MULTIPLE lines per channel)
  std::map<int, std::vector<cvedix_objects::cvedix_line>> all_lines;

  /// @brief Whether to trigger image recording on crossline event
  bool need_record_image;
  /// @brief Whether to trigger video recording on crossline event
  bool need_record_video;

  /// @brief Mutex for thread-safe line updates
  std::mutex lines_mutex;

  /**
   * @brief Check if a point is on one side of a line
   * @param p Point to check
   * @param line Reference line
   * @return true if point is on positive side of line
   */
  bool at_1_side_of_line(cvedix_objects::cvedix_point p,
                         cvedix_objects::cvedix_line line);

protected:
  /**
   * @brief Process frame meta for crossline detection
   * @param meta Frame meta with tracked targets
   * @return Processed meta (may include record control metas)
   */
  virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
      std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
  /**
   * @brief Constructor with multiple lines per channel
   *
   * @param node_name Unique node identifier
   * @param lines Detection lines per channel (channel_id → vector of lines)
   * @param need_record_image Trigger image recording on crossing (default:
   * true)
   * @param need_record_video Trigger video recording on crossing (default:
   * false)
   */
  cvedix_ba_crossline_node(
      std::string node_name,
      std::map<int, std::vector<cvedix_objects::cvedix_line>> lines,
      bool need_record_image = true, bool need_record_video = false);

  /**
   * @brief Constructor with single line per channel (backward compatible)
   *
   * @param node_name Unique node identifier
   * @param lines Detection lines per channel (channel_id → single line)
   * @param need_record_image Trigger image recording on crossing (default:
   * true)
   * @param need_record_video Trigger video recording on crossing (default:
   * false)
   */
  cvedix_ba_crossline_node(std::string node_name,
                           std::map<int, cvedix_objects::cvedix_line> lines,
                           bool need_record_image = true,
                           bool need_record_video = false);

  /// @brief Destructor
  ~cvedix_ba_crossline_node();

  /**
   * @brief Get node description including line configurations
   * @return Human-readable description string
   */
  std::string to_string() override;

  /**
   * @brief Replace all crosslines at runtime (multi-line version)
   * @param lines New detection lines per channel
   * @return true if updated successfully
   */
  bool set_lines(
      const std::map<int, std::vector<cvedix_objects::cvedix_line>> &lines);

  /**
   * @brief Replace all crosslines at runtime (single-line backward compatible)
   * @param lines New detection lines per channel
   * @return true if updated successfully
   */
  bool set_lines(const std::map<int, cvedix_objects::cvedix_line> &lines);

  /**
   * @brief Add a single line to a channel
   * @param channel_id Target channel
   * @param line Line to add
   * @return Index of the added line
   */
  int add_line(int channel_id, const cvedix_objects::cvedix_line &line);

  /**
   * @brief Remove all configured lines
   */
  void clear_lines();

  /**
   * @brief Remove all lines for a specific channel
   * @param channel_id Target channel
   * @return true if channel existed and was removed
   */
  bool remove_channel_lines(int channel_id);

  /**
   * @brief Remove a specific line by channel and index
   * @param channel_id Target channel
   * @param line_index Index of line to remove
   * @return true if line was removed
   */
  bool remove_line(int channel_id, int line_index);

  /**
   * @brief Get number of lines for a channel
   * @param channel_id Target channel
   * @return Number of lines (0 if channel not found)
   */
  size_t get_line_count(int channel_id) const;

  /**
   * @brief Get total crossline count for a specific line
   * @param channel_id Target channel
   * @param line_index Line index
   * @return Crossline count (0 if not found)
   */
  int get_crossline_count(int channel_id, int line_index) const;

  // ========== New Color Configuration APIs ==========

  /**
   * @brief Add a line with full configuration (color, name)
   * @param channel_id Target channel
   * @param config Line configuration with color
   * @return Index of the added line
   */
  int add_line(int channel_id, const crossline_config &config);

  /**
   * @brief Set/update color for a specific line
   * @param channel_id Target channel
   * @param line_index Line index
   * @param color New color in BGR format
   * @return true if updated successfully
   */
  bool set_line_color(int channel_id, int line_index, const cv::Scalar &color);

  /**
   * @brief Get configuration for a specific line
   * @param channel_id Target channel
   * @param line_index Line index
   * @return Line configuration (empty if not found)
   */
  crossline_config get_line_config(int channel_id, int line_index) const;

  /**
   * @brief Get all line configurations for a channel
   * @param channel_id Target channel
   * @return Vector of line configurations
   */
  std::vector<crossline_config> get_all_configs(int channel_id) const;

private:
  /// @brief Line configurations with colors (parallel to all_lines)
  std::map<int, std::vector<crossline_config>> all_configs;
};
} // namespace cvedix_nodes