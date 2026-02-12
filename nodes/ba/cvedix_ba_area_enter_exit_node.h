/**
 * @file cvedix_ba_area_enter_exit_node.h
 * @brief Area enter/exit detection behavior analysis node
 *
 * This node detects when tracked objects enter or exit defined polygonal
 * regions in the video frame. Commonly used for:
 * - Zone intrusion detection
 * - Restricted area monitoring
 * - Customer counting in retail zones
 * - Occupancy monitoring
 *
 * @section area_overview How It Works
 * 1. Receives tracked targets from upstream tracker node
 * 2. Compares each object's current and previous positions
 * 3. Detects enter/exit events for configured polygonal regions
 * 4. Matches events with alert configurations
 * 5. Optionally triggers image/video recording
 *
 * @section area_multichannel Multi-Channel & Multi-Area Support
 * Each channel can have MULTIPLE detection areas.
 * Channels without configured areas skip area detection.
 * Each area can have its own alert configuration.
 *
 * @section area_usage Usage Example
 * @code
 * // Define multiple polygonal areas per channel
 * std::map<int, std::vector<std::vector<cvedix_point>>> areas = {
 *     {0, {
 *         {{100, 100}, {300, 100}, {300, 250}, {100, 250}},  // area 0: entrance zone
 *         {{400, 200}, {700, 200}, {700, 400}, {400, 400}}   // area 1: restricted zone
 *     }}
 * };
 * 
 * auto area_node = std::make_shared<cvedix_ba_area_enter_exit_node>(
 *     "area_monitor",
 *     areas,
 *     true,   // record image on enter/exit
 *     false   // don't record video
 * );
 * area_node->attach_to({tracker_node});
 * @endcode
 *
 * @see cvedix_ba_crossline_node Crossline detection
 * @see cvedix_ba_stop_node Stop detection
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <opencv2/core.hpp>
#include <vector>
#include <set>

namespace cvedix_nodes {

/**
 * @brief Alert configuration for a single area
 */
struct area_alert_config {
  /// @brief Enable enter event alerts
  bool alert_on_enter = true;

  /// @brief Enable exit event alerts
  bool alert_on_exit = true;

  /// @brief Optional name/label for this area (e.g., "entrance", "restricted zone")
  std::string name = "";

  /// @brief Area color in BGR format (default: green) for visualization
  cv::Scalar color = cv::Scalar(0, 255, 0);

  /// @brief Optional anchor point for bbox (default: center)
  cvedix_objects::cvedix_rect_anchor_point anchor_point = cvedix_objects::cvedix_rect_anchor_point::CENTER;

  /// @brief Default constructor
  area_alert_config() = default;

  /// @brief Constructor with defaults (both enter and exit enabled)
  area_alert_config(bool enter, bool exit)
      : alert_on_enter(enter), alert_on_exit(exit), name(""),
        color(cv::Scalar(0, 255, 0)), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}

  /// @brief Constructor with name
  area_alert_config(bool enter, bool exit, const std::string &n)
      : alert_on_enter(enter), alert_on_exit(exit), name(n),
        color(cv::Scalar(0, 255, 0)), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}

  /// @brief Constructor with name and color
  area_alert_config(bool enter, bool exit, const std::string &n,
                    const cv::Scalar &c)
      : alert_on_enter(enter), alert_on_exit(exit), name(n), color(c), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}

  /// @brief Full constructor
  area_alert_config(bool enter, bool exit, const std::string &n,
                    const cv::Scalar &c,
                    cvedix_objects::cvedix_rect_anchor_point anchor)
      : alert_on_enter(enter), alert_on_exit(exit), name(n), color(c), anchor_point(anchor) {}
      
};

/**
 * @brief Area enter/exit detection behavior analysis node
 *
 * Detects when tracked objects enter or exit user-defined polygonal areas.
 * Supports multiple channels with multiple detection areas per channel.
 * Each area can have customized alert configurations.
 *
 * @note Requires tracked objects (must be attached after a tracker node)
 *
 * @see cvedix_node Base class
 */
class cvedix_ba_area_enter_exit_node : public cvedix_node {
private:
  /// @brief Detection areas per channel: channel_id → vector of polygons
  std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> all_areas;

  /// @brief Alert configurations per channel per area
  std::map<int, std::vector<area_alert_config>> all_area_configs;

  /// @brief Track which areas each target was in during the previous frame
  /// channel → track_id → set of area indices
  std::map<int, std::map<int, std::set<int>>> all_previous_area_status;

  /// @brief Last seen frame index per track to allow delayed pruning
  /// channel → track_id → last_seen_frame_index
  std::map<int, std::map<int, int>> all_previous_last_seen_frame;

  /// @brief How many seconds to keep inactive track ids before pruning
  /// (default: 30 seconds)
  int inactive_timeout_seconds = 15;

  /// @brief Whether to trigger image recording on enter/exit event
  bool need_record_image;
  
  /// @brief Whether to trigger video recording on enter/exit event
  bool need_record_video;

  /// @brief Mutex for thread-safe area updates
  std::mutex areas_mutex;

  /**
   * @brief Check if a point is inside a polygon using ray-casting algorithm
   * @param p Point to check
   * @param polygon Polygon to check against
   * @return true if point is inside polygon
   */
  bool is_inside_polygon(const cvedix_objects::cvedix_point &p,
                         const std::vector<cvedix_objects::cvedix_point> &polygon);

  /**
   * @brief Get area indices that contain a given point in bbox
   * @param bbox Bounding box of the target
   * @param areas Vector of polygons
   * @param configs Alert configurations for each area
   * @return Set of area indices containing the point
   */
  std::set<int> get_areas_containing_point(
      const cvedix_objects::cvedix_rect &bbox,
      const std::vector<std::vector<cvedix_objects::cvedix_point>> &areas,
      const std::vector<area_alert_config> &configs);

protected:
  /**
   * @brief Process frame meta for area enter/exit detection
   * @param meta Frame meta with tracked targets
   * @return Processed meta (may include record control metas)
   */
  virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
      std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
  /**
   * @brief Constructor with multiple areas per channel
   *
   * @param node_name Unique node identifier
   * @param areas Detection areas per channel (channel_id → vector of polygons)
   * @param need_record_image Trigger image recording on enter/exit (default: true)
   * @param need_record_video Trigger video recording on enter/exit (default: false)
   */
  cvedix_ba_area_enter_exit_node(
      std::string node_name,
      std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
      bool need_record_image = true, bool need_record_video = false);

    /**
     * @brief Constructor with areas and per-area alert configurations
     *
     * @param node_name Unique node identifier
     * @param areas Detection areas per channel (channel_id → vector of polygons)
     * @param configs Alert configurations per channel per area
     * @param need_record_image Trigger image recording on enter/exit (default: true)
     * @param need_record_video Trigger video recording on enter/exit (default: false)
     */
    cvedix_ba_area_enter_exit_node(
      std::string node_name,
      std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
      std::map<int, std::vector<area_alert_config>> configs,
      bool need_record_image = true, bool need_record_video = false);

  /// @brief Destructor
  ~cvedix_ba_area_enter_exit_node();

  /**
   * @brief Get node description including area configurations
   * @return Human-readable description string
   */
  std::string to_string() override;

  /**
   * @brief Replace all areas at runtime
   * @param areas New detection areas per channel
   * @return true if updated successfully
   */
  bool set_areas(
      const std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> &areas);

  /**
   * @brief Add a single area to a channel
   * @param channel_id Target channel
   * @param area Polygon area to add
   * @return Index of the added area
   */
  int add_area(int channel_id, const std::vector<cvedix_objects::cvedix_point> &area);

  /**
   * @brief Add a single area with configuration
   * @param channel_id Target channel
   * @param area Polygon area to add
   * @param config Alert configuration for the area
   * @return Index of the added area
   */
  int add_area(int channel_id, const std::vector<cvedix_objects::cvedix_point> &area,
               const area_alert_config &config);

  /**
   * @brief Remove all configured areas
   */
  void clear_areas();

  /**
   * @brief Remove all areas for a specific channel
   * @param channel_id Target channel
   * @return true if channel existed and was removed
   */
  bool remove_channel_areas(int channel_id);

  /**
   * @brief Remove a specific area by channel and index
   * @param channel_id Target channel
   * @param area_index Index of area to remove
   * @return true if area was removed
   */
  bool remove_area(int channel_id, int area_index);

  /**
   * @brief Get number of areas for a channel
   * @param channel_id Target channel
   * @return Number of areas (0 if channel not found)
   */
  size_t get_area_count(int channel_id) const;

  /**
   * @brief Set alert configuration for a specific area
   * @param channel_id Target channel
   * @param area_index Area index
   * @param config New alert configuration
   * @return true if updated successfully
   */
  bool set_area_config(int channel_id, int area_index,
                       const area_alert_config &config);

  /**
   * @brief Get alert configuration for a specific area
   * @param channel_id Target channel
   * @param area_index Area index
   * @return Alert configuration (default if not found)
   */
  area_alert_config get_area_config(int channel_id, int area_index) const;

  /**
   * @brief Get all area configurations for a channel
   * @param channel_id Target channel
   * @return Vector of alert configurations
   */
  std::vector<area_alert_config> get_all_configs(int channel_id) const;
};

} // namespace cvedix_nodes
