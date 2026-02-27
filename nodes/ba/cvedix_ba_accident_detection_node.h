/**
 * @file cvedix_ba_accident_detection_node.h
 * @brief Traffic accident detection using tracked object behavior analysis
 *
 * Detects potential traffic accidents by analyzing:
 * 1. Collision: High IoU overlap between two tracked objects
 * 2. Sudden stop: Moving vehicle abruptly stops
 * 3. Trajectory anomaly: Sudden direction change (swerving)
 *
 * @section accident_usage Usage
 * @code
 * auto accident = std::make_shared<cvedix_ba_accident_detection_node>(
 *     "accident",
 *     0.3f,    // collision IoU threshold
 *     5.0f,    // min speed before stop (pixels/frame)
 *     90.0f    // direction change threshold (degrees)
 * );
 * accident->attach_to({tracker});
 * osd->attach_to({accident});
 * @endcode
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include <map>
#include <set>
#include <deque>
#include <mutex>

namespace cvedix_nodes {

class cvedix_ba_accident_detection_node : public cvedix_node {
private:
    /// @brief Minimum IoU overlap to consider a collision
    float collision_iou_thresh;

    /// @brief Minimum average movement (px/frame) to consider object "was moving"
    float min_moving_speed;

    /// @brief Direction change threshold in degrees
    float direction_change_thresh;

    /// @brief Number of frames of history to analyze
    int history_frames;

    /// @brief Track movement history: channel → track_id → list of center points
    std::map<int, std::map<int, std::deque<std::pair<int, int>>>> track_history;

    /// @brief Already alerted collisions (pair of track IDs) to avoid spam
    std::map<int, std::set<std::pair<int, int>>> alerted_collisions;

    /// @brief Already alerted stops (track IDs)
    std::map<int, std::set<int>> alerted_stops;

    /// @brief Accident labels to persist on OSD: channel → track_id → label
    std::map<int, std::map<int, std::string>> accident_labels;

    std::mutex config_mutex;

    /// @brief Calculate IoU between two bounding boxes
    float calculate_iou(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);

    /// @brief Calculate average speed (px/frame) from track history
    float calculate_avg_speed(const std::deque<std::pair<int, int>>& history);

    /// @brief Calculate direction change angle from track history
    float calculate_direction_change(const std::deque<std::pair<int, int>>& history);

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor
     * @param node_name Unique node identifier
     * @param collision_iou IoU threshold for collision (0.15-0.5)
     * @param min_speed Minimum speed to consider moving (px/frame)
     * @param direction_thresh Direction change threshold in degrees
     * @param history_frames Number of frames to keep in history
     */
    cvedix_ba_accident_detection_node(
        std::string node_name,
        float collision_iou = 0.15f,
        float min_speed = 3.0f,
        float direction_thresh = 60.0f,
        int history_frames = 15
    );

    ~cvedix_ba_accident_detection_node();

    std::string to_string() override;
};

} // namespace cvedix_nodes
