/**
 * @file cvedix_ba_fight_detection_node.h
 * @brief Fight/aggressive behavior detection using pose keypoints
 *
 * Detects fighting between two or more people by analyzing:
 * - Proximity between people (close distance)
 * - Rapid limb movement (large frame-to-frame displacement)
 * - Aggressive posture (raised arms/fists)
 *
 * @section fight_algorithm Algorithm
 * 1. Each frame: find pairs of pose_targets within proximity_threshold
 * 2. For each close pair, analyze:
 *    a) Arm speed: wrist displacement between frames
 *    b) Raised arms: wrist higher than shoulder
 *    c) Overall body motion intensity
 * 3. Score each pair; if score > fight_threshold for N frames → alert
 *
 * @section fight_keypoints COCO 17 Keypoints Used
 * | Index | Joint |
 * |-------|-------|
 * | 5,6   | Left/Right Shoulder |
 * | 7,8   | Left/Right Elbow |
 * | 9,10  | Left/Right Wrist |
 * | 11,12 | Left/Right Hip |
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/cvedix_frame_pose_target.h"
#include <map>
#include <mutex>
#include <cmath>
#include <set>

namespace cvedix_nodes {

class cvedix_ba_fight_detection_node : public cvedix_node {
private:
    struct fight_pair_state {
        int consecutive_frames = 0;
        bool alerted = false;
    };

    /// @brief Max pixel distance between two people to be considered "close"
    float proximity_threshold;

    /// @brief Wrist displacement per frame above this = rapid movement (pixels)
    float arm_speed_threshold;

    /// @brief Combined score above this = potential fight
    float fight_score_threshold;

    /// @brief Consecutive frames to confirm fight
    int confirm_frames;

    bool need_record_image;
    bool need_record_video;

    /// @brief Previous frame's wrist positions: pose_index → (lw_x, lw_y, rw_x, rw_y)
    std::map<int, std::tuple<float, float, float, float>> prev_wrists;

    /// @brief Fight state per pair: "i_j" key
    std::map<std::string, fight_pair_state> pair_states;

    std::mutex config_mutex;

    /**
     * @brief Get center point of a pose (average of hip keypoints)
     */
    std::pair<float, float> pose_center(
        const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps) const;

    /**
     * @brief Calculate fight score for a pair of poses
     * @return Score 0.0-1.0 (higher = more likely fighting)
     */
    float calculate_fight_score(
        const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps1,
        const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps2,
        int idx1, int idx2) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor
     *
     * @param node_name Unique node identifier
     * @param proximity_threshold Max distance between people (default: 150px)
     * @param arm_speed_threshold Rapid arm movement threshold (default: 30px/frame)
     * @param fight_score_threshold Score threshold 0-1 (default: 0.5)
     * @param confirm_frames Frames to confirm (default: 3)
     * @param need_record_image Record image on fight detection
     * @param need_record_video Record video on fight detection
     */
    cvedix_ba_fight_detection_node(
        std::string node_name,
        float proximity_threshold = 150.0f,
        float arm_speed_threshold = 30.0f,
        float fight_score_threshold = 0.5f,
        int confirm_frames = 3,
        bool need_record_image = true,
        bool need_record_video = true);

    ~cvedix_ba_fight_detection_node();
    std::string to_string() override;
};

} // namespace cvedix_nodes
