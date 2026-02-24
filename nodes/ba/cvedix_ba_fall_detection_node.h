/**
 * @file cvedix_ba_fall_detection_node.h
 * @brief Fall detection using pose keypoints
 *
 * Detects falls by analyzing body keypoint positions from pose estimation.
 * Uses aspect ratio + body angle heuristics.
 *
 * @section fall_algorithm Algorithm
 * 1. Each frame: check pose_targets[]
 * 2. Calculate body bounding box aspect ratio (width/height)
 * 3. Calculate torso angle (shoulder→hip vector vs vertical)
 * 4. If ratio > threshold AND angle > threshold → potential fall
 * 5. Confirm over N frames to reduce false positives
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/cvedix_frame_pose_target.h"
#include <map>
#include <mutex>
#include <cmath>

namespace cvedix_nodes {

class cvedix_ba_fall_detection_node : public cvedix_node {
private:
    struct fall_state {
        int consecutive_fall_frames = 0;
        bool alerted = false;
    };

    /// @brief Aspect ratio threshold (width/height > this → horizontal posture)
    float fall_ratio_threshold;

    /// @brief Torso angle threshold in degrees (> this → tilted/fallen)
    float fall_angle_threshold;

    /// @brief Number of consecutive frames to confirm fall
    int confirm_frames;

    bool need_record_image;
    bool need_record_video;

    /// @brief Track fall state per pose index
    std::map<int, fall_state> pose_states;

    std::mutex config_mutex;

    /**
     * @brief Calculate bounding box from keypoints
     * @return (x,y,w,h) or (-1,-1,-1,-1) if insufficient keypoints
     */
    std::tuple<int, int, int, int> keypoints_bbox(
        const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps,
        float min_score = 0.3f) const;

    /**
     * @brief Calculate torso angle from keypoints
     * @return Angle in degrees from vertical (0=standing, 90=lying)
     */
    float torso_angle(
        const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor
     *
     * @param node_name Unique node identifier
     * @param fall_ratio_threshold Width/height ratio threshold (default: 1.2)
     * @param fall_angle_threshold Torso angle threshold in degrees (default: 45)
     * @param confirm_frames Frames to confirm fall (default: 5)
     * @param need_record_image Record image on fall
     * @param need_record_video Record video on fall
     */
    cvedix_ba_fall_detection_node(
        std::string node_name,
        float fall_ratio_threshold = 1.2f,
        float fall_angle_threshold = 45.0f,
        int confirm_frames = 5,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_fall_detection_node();
    std::string to_string() override;
};

} // namespace cvedix_nodes
