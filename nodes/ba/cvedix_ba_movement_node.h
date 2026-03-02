/**
 * @file cvedix_ba_movement_node.h
 * @brief Movement detection behavior analysis node
 *
 * This node triggers when a relevant object moves inside a defined area.
 * Relevant objects can be configured to include people, vehicles, animals,
 * custom classes, or unidentified objects caused by motion.
 *
 * @section movement_overview How It Works
 * 1. Receives tracked targets or motion objects from upstream node
 * 2. Checks if relevant objects are moving inside configured areas
 * 3. Triggers movement alerts based on configuration
 * 4. Optionally triggers image/video recording
 *
 * @section movement_multichannel Multi-Channel & Multi-Area Support
 * Each channel can have MULTIPLE detection areas.
 * Channels without configured areas skip detection.
 * Each area can have its own alert configuration.
 *
 * @section movement_usage Usage Example
 * @code
 * // Define multiple polygonal areas per channel
 * std::map<int, std::vector<std::vector<cvedix_point>>> areas = {
 *     {0, {
 *         {{100, 100}, {300, 100}, {300, 250}, {100, 250}},  // area 0: monitoring zone
 *         {{400, 200}, {700, 200}, {700, 400}, {400, 400}}   // area 1: restricted zone
 *     }}
 * };
 * 
 * auto movement_node = std::make_shared<cvedix_ba_movement_node>(
 *     "movement_detector",
 *     areas,
 *     configs,
 *     true,   // record image on movement
 *     false   // don't record video
 * );
 * movement_node->attach_to({tracker_node});
 * @endcode
 *
 * @see cvedix_ba_area_enter_exit_node Area enter/exit detection
 * @see cvedix_ba_crossline_node Crossline detection
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
#include <string>

namespace cvedix_nodes {

/**
 * @brief Alert configuration for a single movement detection area
 */
struct movement_alert_config {
  /// @brief Enable movement event alerts
  bool alert_on_movement = true;

  /// @brief Optional name/label for this area
  std::string name = "";

  /// @brief Area color in BGR format (default: blue) for visualization
  cv::Scalar color = cv::Scalar(255, 0, 0);

  /// @brief Optional anchor point for bbox (default: center)
  cvedix_objects::cvedix_rect_anchor_point anchor_point = cvedix_objects::cvedix_rect_anchor_point::CENTER;

  /// @brief Relevant object classes (person, vehicle, animal, custom, unidentified)
  /// If empty, matches all object classes
  std::set<std::string> relevant_classes;

  /// @brief Default constructor
  movement_alert_config() = default;

  /// @brief Full constructor
  movement_alert_config(bool movement, const std::string &n, const cv::Scalar &c, cvedix_objects::cvedix_rect_anchor_point anchor, const std::set<std::string> &classes)
      : alert_on_movement(movement), name(n), color(c), anchor_point(anchor), relevant_classes(classes) {}
};

/**
 * @brief Movement detection behavior analysis node
 *
 * Detects when tracked objects move inside user-defined polygonal areas.
 * Supports multiple channels with multiple detection areas per channel.
 * Each area can have customized alert configurations and object class filters.
 *
 * @note Requires tracked objects (must be attached after a tracker node)
 *
 * @see cvedix_node Base class
 */
class cvedix_ba_movement_node : public cvedix_node {
private:
  /// @brief Detection areas per channel: channel_id → vector of polygons
  std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> all_areas;

  /// @brief Alert configurations per channel per area
  std::map<int, std::vector<movement_alert_config>> all_area_configs;

  /// @brief Whether to trigger image recording on movement event
  bool need_record_image;
  
  /// @brief Whether to trigger video recording on movement event
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
   * @param object_class Object class label to filter by relevant_classes
   * @return Set of area indices containing the point and matching class filter
   */
  std::set<int> get_areas_containing_point(
      const cvedix_objects::cvedix_rect &bbox,
      const std::vector<std::vector<cvedix_objects::cvedix_point>> &areas,
      const std::vector<movement_alert_config> &configs,
      const std::string &object_class);

protected:
  /**
   * @brief Process frame meta for movement detection
   * @param meta Frame meta with tracked targets
   * @return Processed meta (may include record control metas)
   */
  virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
      std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
  /**
   * @brief Constructor with areas and per-area alert configurations
   *
   * @param node_name Unique node identifier
   * @param areas Detection areas per channel (channel_id → vector of polygons)
   * @param configs Alert configurations per channel per area
   * @param need_record_image Trigger image recording on movement (default: true)
   * @param need_record_video Trigger video recording on movement (default: false)
   */
  cvedix_ba_movement_node(
      std::string node_name,
      std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
      std::map<int, std::vector<movement_alert_config>> configs,
      bool need_record_image = true, bool need_record_video = false);

  /// @brief Destructor
  ~cvedix_ba_movement_node();

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
  bool set_areas(const std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> &areas);

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
               const movement_alert_config &config);

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
                      const movement_alert_config &config);

  /**
   * @brief Get alert configuration for a specific area
   * @param channel_id Target channel
   * @param area_index Area index
   * @return Alert configuration (default if not found)
   */
  movement_alert_config get_area_config(int channel_id, int area_index) const;

  /**
   * @brief Get all area configurations for a channel
   * @param channel_id Target channel
   * @return Vector of alert configurations
   */
  std::vector<movement_alert_config> get_all_configs(int channel_id) const;
};

} // namespace cvedix_nodes
