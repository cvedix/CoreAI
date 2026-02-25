/**
 * @file cvedix_ba_line_illegal_uturn_node.h
 * @brief Illegal U-turn detection using trajectory angle analysis
 *
 * Detects vehicles making U-turns at locations where U-turns are prohibited.
 * Uses trajectory heading angle analysis over recent track history.
 *
 * @section ut_algorithm Algorithm
 * 1. Define no-U-turn zone polygon
 * 2. Compute heading angle from recent N track positions
 * 3. If total angle change > threshold (135°) within zone → U-turn detected
 * 4. Requires sufficient track history for accuracy
 *
 * @note Higher false positive rate than other BA nodes due to tracking noise
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <set>
#include <cmath>
#include <opencv2/core.hpp>

namespace cvedix_nodes {

/**
 * @brief Configuration for illegal U-turn detection
 */
struct uturn_config {
    /// @brief No-U-turn zone polygon (empty = whole frame)
    std::vector<cvedix_objects::cvedix_point> zone_polygon;

    /// @brief Number of recent track positions to analyze
    int track_window = 20;

    /// @brief Minimum total angle change (degrees) to detect U-turn
    double angle_threshold_degrees = 135.0;

    /// @brief Minimum pixel displacement per step to consider valid movement
    double min_movement_per_step = 3.0;

    /// @brief Vehicle class IDs to monitor (empty = all)
    std::set<int> vehicle_class_ids;

    /// @brief Display color
    cv::Scalar color = cv::Scalar(255, 0, 255);

    /// @brief Configuration name
    std::string name = "";

    uturn_config() = default;
};

/**
 * @brief Illegal U-turn detection node
 *
 * Uses trajectory heading analysis to detect U-turns in no-U-turn zones.
 *
 * @note Accuracy depends on camera angle and tracking quality
 */
class cvedix_ba_line_illegal_uturn_node : public cvedix_node {
private:
    /// @brief Config per channel
    std::map<int, uturn_config> all_configs;

    /// @brief Already alerted: channel → set of track_ids
    std::map<int, std::set<int>> alerted_tracks;

    bool need_record_image;
    bool need_record_video;

    std::mutex config_mutex;

    bool is_inside_polygon(const cvedix_objects::cvedix_point& p,
                           const std::vector<cvedix_objects::cvedix_point>& polygon) const;

    /// @brief Analyze track trajectory for U-turn
    bool detect_uturn(const std::vector<cvedix_objects::cvedix_rect>& tracks,
                      const uturn_config& config) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    cvedix_ba_line_illegal_uturn_node(
        std::string node_name,
        std::map<int, uturn_config> configs,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_line_illegal_uturn_node();
    std::string to_string() override;

    /// @brief Update config at runtime
    bool set_config(int channel_id, const uturn_config& config);
};

} // namespace cvedix_nodes
