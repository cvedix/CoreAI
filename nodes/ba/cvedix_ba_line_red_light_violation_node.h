/**
 * @file cvedix_ba_line_red_light_violation_node.h
 * @brief Red light running detection with stop-line violation sub-event
 *
 * Detects vehicles crossing a stop line AND entering an intersection
 * zone while the traffic signal is RED. Also detects stop-line-only
 * violations (vehicle crosses stop line but stops before intersection).
 *
 * @section rl_algorithm Algorithm
 * 1. External system sets signal state via set_signal_state() API
 * 2. When RED:
 *    - Vehicle crosses stop_line → mark as potential violator
 *    - Vehicle enters intersection_area → confirm RED LIGHT violation
 *    - Vehicle crosses stop_line but stays outside intersection → STOP_LINE violation
 * 3. Grace period after signal change to avoid false positives
 *
 * @section rl_usage Usage
 * @code
 * red_light_config cfg;
 * cfg.stop_line = cvedix_line({0,300},{640,300});
 * cfg.intersection_area = {{0,0},{640,0},{640,300},{0,300}};
 *
 * auto node = std::make_shared<cvedix_ba_line_red_light_violation_node>(
 *     "red_light", {{0, cfg}});
 * node->attach_to({tracker_node});
 * node->set_signal_state(0, traffic_signal_state::RED);
 * @endcode
 *
 * @see cvedix_ba_line_crossline_node Line crossing base logic
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
#include <chrono>
#include <opencv2/core.hpp>

namespace cvedix_nodes {

/**
 * @brief Traffic signal state
 */
enum class traffic_signal_state {
    GREEN = 0,
    YELLOW = 1,
    RED = 2,
    UNKNOWN = 3
};

/**
 * @brief Configuration for red-light violation detection
 */
struct red_light_config {
    /// @brief Stop line geometry
    cvedix_objects::cvedix_line stop_line;

    /// @brief Intersection area polygon (beyond stop line)
    std::vector<cvedix_objects::cvedix_point> intersection_area;

    /// @brief Vehicle class IDs to monitor (empty = all)
    std::set<int> vehicle_class_ids;

    /// @brief Grace period after signal change (seconds)
    double grace_period_seconds = 2.0;

    /// @brief Display color
    cv::Scalar color = cv::Scalar(0, 0, 255);

    /// @brief Configuration name
    std::string name = "";

    red_light_config() = default;

    red_light_config(
        const cvedix_objects::cvedix_line& line,
        const std::vector<cvedix_objects::cvedix_point>& area,
        const std::set<int>& classes = {},
        double grace = 2.0,
        const std::string& n = "",
        const cv::Scalar& c = cv::Scalar(0, 0, 255))
        : stop_line(line), intersection_area(area),
          vehicle_class_ids(classes), grace_period_seconds(grace),
          name(n), color(c) {}
};

/**
 * @brief Red light violation detection node
 *
 * Two-level violation detection:
 * - STOP_LINE: crossed stop line on red but didn't enter intersection
 * - RED_LIGHT: crossed stop line AND entered intersection on red
 *
 * @note Signal state must be set externally via set_signal_state()
 */
class cvedix_ba_line_red_light_violation_node : public cvedix_node {
private:
    /// @brief Config per channel
    std::map<int, red_light_config> all_configs;

    /// @brief Current signal state per channel
    std::map<int, traffic_signal_state> signal_states;

    /// @brief Frame when signal last changed to RED per channel
    std::map<int, int> red_start_frame;

    /// @brief Tracks that crossed stop line: channel → set of track_ids
    std::map<int, std::set<int>> crossed_stop_line;

    /// @brief Already alerted for RED_LIGHT: channel → set of track_ids
    std::map<int, std::set<int>> alerted_red_light;

    /// @brief Already alerted for STOP_LINE: channel → set of track_ids
    std::map<int, std::set<int>> alerted_stop_line;

    bool need_record_image;
    bool need_record_video;
    int fps = 30;

    std::mutex config_mutex;

    bool at_1_side_of_line(cvedix_objects::cvedix_point p,
                           cvedix_objects::cvedix_line line);

    bool is_inside_polygon(const cvedix_objects::cvedix_point& p,
                           const std::vector<cvedix_objects::cvedix_point>& polygon);

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    cvedix_ba_line_red_light_violation_node(
        std::string node_name,
        std::map<int, red_light_config> configs,
        bool need_record_image = true,
        bool need_record_video = true);

    ~cvedix_ba_line_red_light_violation_node();
    std::string to_string() override;

    /**
     * @brief Set traffic signal state for a channel
     * @param channel_id Target channel
     * @param state Signal state (GREEN/YELLOW/RED)
     */
    void set_signal_state(int channel_id, traffic_signal_state state);

    /// @brief Get current signal state
    traffic_signal_state get_signal_state(int channel_id) const;

    /// @brief Set FPS for grace period calculation
    void set_fps(int f);
};

} // namespace cvedix_nodes
