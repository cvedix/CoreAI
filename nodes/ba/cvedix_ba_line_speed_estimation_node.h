/**
 * @file cvedix_ba_line_speed_estimation_node.h
 * @brief Speed estimation using two detection lines and calibration
 *
 * Measures object speed by timing travel between two parallel lines
 * with a known real-world distance.
 *
 * @section speed_algorithm Algorithm
 * 1. When target crosses line1 → record timestamp (frame_index)
 * 2. When target crosses line2 → calculate speed = distance / time
 * 3. Alert if speed exceeds speed_limit
 *
 * @section speed_usage Usage
 * @code
 * // Two parallel lines 50px apart, real distance = 5 meters
 * std::map<int, std::pair<cvedix_line, cvedix_line>> line_pairs = {
 *     {0, {cvedix_line({0,200},{640,200}), cvedix_line({0,300},{640,300})}}
 * };
 * std::map<int, double> px_to_m = {{0, 0.1}};  // 1px = 0.1m
 * auto speed = std::make_shared<cvedix_ba_line_speed_estimation_node>(
 *     "speed", line_pairs, px_to_m, 60.0);
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

namespace cvedix_nodes {

/**
 * @brief Speed estimation behavior analysis node
 */
class cvedix_ba_line_speed_estimation_node : public cvedix_node {
private:
    /// @brief Line pairs per channel: channel_id → (entry_line, exit_line)
    std::map<int, std::pair<cvedix_objects::cvedix_line, cvedix_objects::cvedix_line>> all_line_pairs;

    /// @brief Pixel-to-meter calibration per channel
    std::map<int, double> all_pixel_to_meter;

    /// @brief Speed limit in km/h
    double speed_limit_kmh;

    /// @brief FPS for time calculation
    int fps = 30;

    /// @brief Whether to trigger recording
    bool need_record_image;
    bool need_record_video;

    /// @brief Track crossing state: channel → track_id → frame_index when crossed line1
    std::map<int, std::map<int, int>> line1_cross_frame;

    /// @brief Tracks that already triggered (avoid duplicate alerts)
    std::map<int, std::map<int, bool>> alerted_tracks;

    /// @brief Stored speed labels per track for persistent OSD display
    std::map<int, std::map<int, std::string>> track_speeds;

    /// @brief Mutex for thread-safe updates
    std::mutex config_mutex;

    /**
     * @brief Check which side of a line a point is on
     */
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
     * @param line_pairs Entry/exit line pairs per channel
     * @param pixel_to_meter Calibration ratio per channel (pixels → meters)
     * @param speed_limit_kmh Speed limit in km/h (alerts above this)
     * @param need_record_image Trigger image recording on speed violation
     * @param need_record_video Trigger video recording on speed violation
     */
    cvedix_ba_line_speed_estimation_node(
        std::string node_name,
        std::map<int, std::pair<cvedix_objects::cvedix_line, cvedix_objects::cvedix_line>> line_pairs,
        std::map<int, double> pixel_to_meter,
        double speed_limit_kmh = 60.0,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_line_speed_estimation_node();

    std::string to_string() override;

    /// @brief Set FPS for time calculation
    void set_fps(int fps);

    /// @brief Get current speed limit
    double get_speed_limit() const { return speed_limit_kmh; }

    /// @brief Set speed limit at runtime
    void set_speed_limit(double limit_kmh);
};

} // namespace cvedix_nodes
