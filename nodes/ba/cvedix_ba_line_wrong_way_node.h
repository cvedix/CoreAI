/**
 * @file cvedix_ba_line_wrong_way_node.h
 * @brief Wrong-way driving detection using multiple confirmation lines
 *
 * Detects vehicles traveling in the forbidden direction on a one-way road.
 * Uses ≥2 parallel lines to confirm wrong-way with high confidence.
 *
 * @section ww_algorithm Algorithm
 * 1. Place ≥2 parallel detection lines along the road
 * 2. Each line has a single allowed crossing direction
 * 3. If a target crosses ≥min_lines_crossed in the WRONG direction → violation
 * 4. Supports exempt vehicle classes (e.g., emergency vehicles)
 *
 * @section ww_usage Usage
 * @code
 * std::map<int, wrong_way_config> configs = {
 *     {0, {
 *         {cvedix_line({0,200},{640,200}), cvedix_line({0,300},{640,300})},
 *         cvedix_ba_direct_type::IN,   // only IN allowed
 *         2                             // min 2 lines crossed to confirm
 *     }}
 * };
 * auto node = std::make_shared<cvedix_ba_line_wrong_way_node>("wrong_way", configs);
 * node->attach_to({tracker_node});
 * @endcode
 *
 * @see cvedix_ba_line_direction_violation_node Single-line direction check
 * @see cvedix_ba_line_crossline_node Basic crossline detection
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <set>

namespace cvedix_nodes {

/**
 * @brief Configuration for wrong-way detection per channel
 */
struct wrong_way_config {
    /// @brief Detection lines (≥2 recommended for confirmation)
    std::vector<cvedix_objects::cvedix_line> detection_lines;

    /// @brief The only allowed crossing direction
    cvedix_objects::cvedix_ba_direct_type allowed_direction =
        cvedix_objects::cvedix_ba_direct_type::IN;

    /// @brief Minimum lines that must be crossed in wrong direction to trigger
    int min_lines_crossed = 2;

    /// @brief Vehicle class IDs exempt from wrong-way check (e.g., emergency)
    std::set<int> exempt_class_ids;

    /// @brief Display color for OSD
    cv::Scalar color = cv::Scalar(0, 0, 255);

    /// @brief Configuration name/label
    std::string name = "";

    wrong_way_config() = default;

    wrong_way_config(
        const std::vector<cvedix_objects::cvedix_line>& lines,
        cvedix_objects::cvedix_ba_direct_type allowed,
        int min_crossed = 2,
        const std::set<int>& exempt = {},
        const std::string& n = "",
        const cv::Scalar& c = cv::Scalar(0, 0, 255))
        : detection_lines(lines), allowed_direction(allowed),
          min_lines_crossed(min_crossed), exempt_class_ids(exempt),
          name(n), color(c) {}
};

/**
 * @brief Wrong-way driving detection node
 *
 * Enhanced version of direction_violation_node with multi-line confirmation
 * and vehicle class filtering to reduce false positives.
 *
 * @note Requires tracked objects (attach after tracker node)
 */
class cvedix_ba_line_wrong_way_node : public cvedix_node {
private:
    /// @brief Config per channel
    std::map<int, wrong_way_config> all_configs;

    /// @brief Track wrong-direction crossing count: channel → track_id → count
    std::map<int, std::map<int, int>> wrong_cross_count;

    /// @brief Already alerted track IDs per channel
    std::map<int, std::set<int>> alerted_tracks;

    bool need_record_image;
    bool need_record_video;

    std::mutex config_mutex;

    bool at_1_side_of_line(cvedix_objects::cvedix_point p,
                           cvedix_objects::cvedix_line line);

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor
     * @param node_name Unique node identifier
     * @param configs Wrong-way config per channel
     * @param need_record_image Record image on violation
     * @param need_record_video Record video on violation
     */
    cvedix_ba_line_wrong_way_node(
        std::string node_name,
        std::map<int, wrong_way_config> configs,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_line_wrong_way_node();
    std::string to_string() override;

    /// @brief Update config at runtime
    bool set_config(int channel_id, const wrong_way_config& config);
};

} // namespace cvedix_nodes
