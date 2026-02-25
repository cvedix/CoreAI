/**
 * @file cvedix_ba_area_illegal_turn_node.h
 * @brief Illegal turn detection at intersections with per-vehicle-type rules
 *
 * Detects vehicles making turns that are not allowed for their vehicle type
 * at intersections. Uses entry/exit direction analysis through approach and
 * exit lines to determine the turn direction.
 *
 * @section it_algorithm Algorithm
 * 1. Vehicle enters intersection polygon → detect approach direction
 * 2. Vehicle exits intersection polygon → detect exit direction
 * 3. Compute turn type (STRAIGHT/LEFT/RIGHT) from approach→exit
 * 4. Check against per-class turn rules → if not allowed, violation
 *
 * @see cvedix_ba_line_direction_violation_node Direction violation base
 * @see cvedix_ba_area_enter_exit_node Area detection base
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
#include <opencv2/core.hpp>
#include <cmath>

namespace cvedix_nodes {

/**
 * @brief Turn direction types
 */
enum class turn_direction {
    STRAIGHT = 0,
    LEFT = 1,
    RIGHT = 2,
    U_TURN = 3,
    UNKNOWN = 4
};

/**
 * @brief Turn rule: which turns are allowed for which vehicle classes from a given approach
 */
struct turn_rule {
    /// @brief Approach line index (which direction vehicle came from)
    int approach_index;

    /// @brief Allowed turn directions for this approach
    std::set<turn_direction> allowed_turns;

    /// @brief Vehicle class IDs this rule applies to (empty = all classes)
    std::set<int> vehicle_class_ids;

    turn_rule() = default;

    turn_rule(int approach, const std::set<turn_direction>& turns,
              const std::set<int>& classes = {})
        : approach_index(approach), allowed_turns(turns),
          vehicle_class_ids(classes) {}
};

/**
 * @brief Configuration for illegal turn detection at an intersection
 */
struct illegal_turn_config {
    /// @brief Intersection area polygon
    std::vector<cvedix_objects::cvedix_point> intersection_area;

    /// @brief Approach/exit lines (typically 2-4, one per road arm)
    /// Lines define the boundaries between the intersection and each road
    std::vector<cvedix_objects::cvedix_line> boundary_lines;

    /// @brief Turn rules: per-approach, per-class allowed turns
    std::vector<turn_rule> rules;

    /// @brief Minimum frames inside intersection before checking (filter transients)
    int min_frames_inside = 5;

    /// @brief Display color
    cv::Scalar color = cv::Scalar(0, 165, 255);

    /// @brief Configuration name
    std::string name = "";

    illegal_turn_config() = default;
};

/**
 * @brief Illegal turn detection node
 *
 * Tracks vehicles through intersections and determines if their
 * turn direction is allowed based on configured rules.
 */
class cvedix_ba_area_illegal_turn_node : public cvedix_node {
private:
    /// @brief Configs per channel
    std::map<int, illegal_turn_config> all_configs;

    /// @brief State tracking for vehicles in intersection
    struct vehicle_state {
        int approach_line = -1;     ///< Which line was crossed to enter
        int frames_inside = 0;      ///< How many frames inside intersection
        bool was_inside = false;    ///< Previous frame inside status
    };

    /// @brief Vehicle states: channel → track_id → state
    std::map<int, std::map<int, vehicle_state>> vehicle_states;

    /// @brief Already alerted: channel → set of track_ids
    std::map<int, std::set<int>> alerted_tracks;

    bool need_record_image;
    bool need_record_video;

    std::mutex config_mutex;

    bool is_inside_polygon(const cvedix_objects::cvedix_point& p,
                           const std::vector<cvedix_objects::cvedix_point>& polygon) const;

    bool at_1_side_of_line(cvedix_objects::cvedix_point p,
                           cvedix_objects::cvedix_line line) const;

    /// @brief Determine which boundary line was crossed
    int find_crossed_line(const cvedix_objects::cvedix_point& curr,
                          const cvedix_objects::cvedix_point& prev,
                          const std::vector<cvedix_objects::cvedix_line>& lines) const;

    /// @brief Determine turn direction from approach→exit line indices
    turn_direction compute_turn(int approach_idx, int exit_idx, int num_lines) const;

    /// @brief Get turn label string
    std::string turn_to_string(turn_direction dir) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    cvedix_ba_area_illegal_turn_node(
        std::string node_name,
        std::map<int, illegal_turn_config> configs,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_area_illegal_turn_node();
    std::string to_string() override;
};

} // namespace cvedix_nodes
