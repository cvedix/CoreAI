/**
 * @file cvedix_ba_line_direction_violation_node.h
 * @brief Direction violation detection using crosslines
 *
 * Detects objects moving in a forbidden direction across a line.
 * 
 * @section dir_algorithm Algorithm
 * 1. Define detection lines with allowed direction (IN or OUT)
 * 2. If target crosses line in the OPPOSITE direction → violation
 *
 * @section dir_usage Usage
 * @code
 * std::map<int, std::vector<cvedix_line>> lines = {
 *     {0, {cvedix_line({0,300},{640,300})}}
 * };
 * // Only allow crossing from top to bottom (IN)
 * std::map<int, std::vector<cvedix_ba_direct_type>> dirs = {
 *     {0, {cvedix_ba_direct_type::IN}}
 * };
 * auto dir_node = std::make_shared<cvedix_ba_line_direction_violation_node>(
 *     "dir_violation", lines, dirs);
 * @endcode
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
 * @brief Direction violation detection node
 */
class cvedix_ba_line_direction_violation_node : public cvedix_node {
private:
    /// @brief Detection lines per channel
    std::map<int, std::vector<cvedix_objects::cvedix_line>> all_lines;

    /// @brief Allowed direction per channel per line index
    std::map<int, std::vector<cvedix_objects::cvedix_ba_direct_type>> all_allowed_dirs;

    bool need_record_image;
    bool need_record_video;

    /// @brief Already alerted track IDs per channel (avoid duplicates)
    std::map<int, std::set<int>> alerted_tracks;

    std::mutex config_mutex;

    bool at_1_side_of_line(cvedix_objects::cvedix_point p,
                           cvedix_objects::cvedix_line line);

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor
     *
     * @param node_name Unique node identifier
     * @param lines Detection lines per channel
     * @param allowed_directions Allowed direction per channel per line
     * @param need_record_image Record image on violation
     * @param need_record_video Record video on violation
     */
    cvedix_ba_line_direction_violation_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_line>> lines,
        std::map<int, std::vector<cvedix_objects::cvedix_ba_direct_type>> allowed_directions,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_line_direction_violation_node();
    std::string to_string() override;
};

} // namespace cvedix_nodes
