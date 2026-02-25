/**
 * @file cvedix_ba_area_lane_violation_node.h
 * @brief Lane violation detection — wrong vehicle type in restricted lane
 *
 * Detects when a vehicle type drives in a lane not designated for it.
 * Example: motorcycle in car-only lane, truck in light-vehicle lane.
 *
 * @section lv_algorithm Algorithm
 * 1. Define lane polygons with allowed vehicle class IDs
 * 2. If a vehicle of wrong class stays in the lane ≥ min_frames → violation
 * 3. Frame threshold avoids false positives during legitimate lane changes
 *
 * @section lv_usage Usage
 * @code
 * lane_config lane1;
 * lane1.lane_polygon = {{50,200},{300,200},{300,400},{50,400}};
 * lane1.allowed_class_ids = {0, 1}; // car, bus
 * lane1.lane_name = "Car Lane";
 *
 * std::map<int, std::vector<lane_config>> configs = {{0, {lane1}}};
 * auto node = std::make_shared<cvedix_ba_area_lane_violation_node>(
 *     "lane_check", configs);
 * node->attach_to({tracker_node});
 * @endcode
 *
 * @see cvedix_ba_area_enter_exit_node Area detection base
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <set>
#include <opencv2/core.hpp>

namespace cvedix_nodes {

/**
 * @brief Configuration for a single lane
 */
struct lane_config {
    /// @brief Lane polygon vertices
    std::vector<cvedix_objects::cvedix_point> lane_polygon;

    /// @brief Vehicle class IDs allowed in this lane (empty = all allowed)
    std::set<int> allowed_class_ids;

    /// @brief Lane display name
    std::string lane_name = "";

    /// @brief Display color for OSD
    cv::Scalar color = cv::Scalar(0, 255, 255);

    /// @brief Minimum consecutive frames in wrong lane to trigger violation
    int min_frames_in_lane = 30;

    /// @brief Anchor point for position check
    cvedix_objects::cvedix_rect_anchor_point anchor_point =
        cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM;

    lane_config() = default;

    lane_config(const std::vector<cvedix_objects::cvedix_point>& polygon,
                const std::set<int>& allowed,
                const std::string& name = "",
                int min_frames = 30,
                const cv::Scalar& c = cv::Scalar(0, 255, 255))
        : lane_polygon(polygon), allowed_class_ids(allowed),
          lane_name(name), min_frames_in_lane(min_frames), color(c) {}
};

/**
 * @brief Lane violation detection node
 *
 * Monitors vehicle class vs lane assignment. Triggers when wrong
 * vehicle type stays in a restricted lane for sufficient duration.
 *
 * @note Requires tracked objects with class labels (attach after tracker)
 */
class cvedix_ba_area_lane_violation_node : public cvedix_node {
private:
    /// @brief Lane configs per channel: channel_id → vector of lane_config
    std::map<int, std::vector<lane_config>> all_lane_configs;

    /// @brief Frames in wrong lane: channel → track_id → lane_index → frame_count
    std::map<int, std::map<int, std::map<int, int>>> frames_in_wrong_lane;

    /// @brief Already alerted: channel → track_id → lane_index
    std::map<int, std::map<int, std::set<int>>> alerted_tracks;

    bool need_record_image;
    bool need_record_video;

    std::mutex config_mutex;

    bool is_inside_polygon(const cvedix_objects::cvedix_point& p,
                           const std::vector<cvedix_objects::cvedix_point>& polygon) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor
     * @param node_name Unique node identifier
     * @param lane_configs Lane configurations per channel
     * @param need_record_image Record image on violation
     * @param need_record_video Record video on violation
     */
    cvedix_ba_area_lane_violation_node(
        std::string node_name,
        std::map<int, std::vector<lane_config>> lane_configs,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_area_lane_violation_node();
    std::string to_string() override;

    /// @brief Update lanes at runtime
    bool set_lanes(int channel_id, const std::vector<lane_config>& configs);
    void clear_lanes();
};

} // namespace cvedix_nodes
