#include "cvedix_ba_fall_detection_node.h"

namespace cvedix_nodes {

cvedix_ba_fall_detection_node::cvedix_ba_fall_detection_node(
    std::string node_name,
    float fall_ratio_threshold,
    float fall_angle_threshold,
    int confirm_frames,
    bool need_record_image,
    bool need_record_video)
    : cvedix_node(node_name),
      fall_ratio_threshold(fall_ratio_threshold),
      fall_angle_threshold(fall_angle_threshold),
      confirm_frames(confirm_frames),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] fall_detection(ratio=%.1f, angle=%.1f°, confirm=%d frames)",
        node_name.c_str(), fall_ratio_threshold, fall_angle_threshold,
        confirm_frames));
    this->initialized();
}

cvedix_ba_fall_detection_node::~cvedix_ba_fall_detection_node() {
    deinitialized();
}

std::string cvedix_ba_fall_detection_node::to_string() {
    return cvedix_utils::string_format(
        "fall_detection(ratio_thr=%.1f, angle_thr=%.1f°, confirm=%d)",
        fall_ratio_threshold, fall_angle_threshold, confirm_frames);
}

std::tuple<int, int, int, int> cvedix_ba_fall_detection_node::keypoints_bbox(
    const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps,
    float min_score) const {
    int min_x = INT_MAX, min_y = INT_MAX;
    int max_x = 0, max_y = 0;
    int valid = 0;

    for (const auto& kp : kps) {
        if (kp.score < min_score) continue;
        if (kp.x < min_x) min_x = kp.x;
        if (kp.y < min_y) min_y = kp.y;
        if (kp.x > max_x) max_x = kp.x;
        if (kp.y > max_y) max_y = kp.y;
        valid++;
    }

    if (valid < 3) return {-1, -1, -1, -1};

    int w = max_x - min_x;
    int h = max_y - min_y;
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    return {min_x, min_y, w, h};
}

float cvedix_ba_fall_detection_node::torso_angle(
    const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps) const {
    // COCO keypoint indices:
    // 5 = left_shoulder, 6 = right_shoulder
    // 11 = left_hip, 12 = right_hip
    // YOLOv8-pose uses same COCO 17 keypoint layout

    float shoulder_x = 0, shoulder_y = 0;
    float hip_x = 0, hip_y = 0;
    int shoulder_count = 0, hip_count = 0;
    float min_score = 0.3f;

    for (const auto& kp : kps) {
        if (kp.score < min_score) continue;
        if (kp.point_type == 5 || kp.point_type == 6) {
            shoulder_x += kp.x;
            shoulder_y += kp.y;
            shoulder_count++;
        } else if (kp.point_type == 11 || kp.point_type == 12) {
            hip_x += kp.x;
            hip_y += kp.y;
            hip_count++;
        }
    }

    if (shoulder_count == 0 || hip_count == 0) return 0.0f;

    shoulder_x /= shoulder_count;
    shoulder_y /= shoulder_count;
    hip_x /= hip_count;
    hip_y /= hip_count;

    // Vector from hip to shoulder
    float dx = shoulder_x - hip_x;
    float dy = shoulder_y - hip_y;  // Note: y increases downward in image

    // Angle from vertical (0° = upright, 90° = horizontal)
    // Vertical vector is (0, -1) in image coords
    float angle_rad = std::atan2(std::abs(dx), std::abs(dy));
    return angle_rad * 180.0f / M_PI;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_fall_detection_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;

    // Process each pose target
    for (size_t pi = 0; pi < meta->pose_targets.size(); pi++) {
        const auto& pose = meta->pose_targets[pi];
        const auto& kps = pose->key_points;

        if (kps.size() < 6) continue;  // Need minimum keypoints

        // Calculate bbox aspect ratio
        auto [bx, by, bw, bh] = keypoints_bbox(kps);
        if (bx < 0) continue;

        float ratio = static_cast<float>(bw) / static_cast<float>(bh);

        // Calculate torso angle
        float angle = torso_angle(kps);

        // Fall detection logic
        bool is_fall_posture = (ratio > fall_ratio_threshold) && (angle > fall_angle_threshold);

        auto& state = pose_states[static_cast<int>(pi)];

        if (is_fall_posture) {
            state.consecutive_fall_frames++;
        } else {
            state.consecutive_fall_frames = 0;
            state.alerted = false;
        }

        // Confirm fall after N consecutive frames
        if (state.consecutive_fall_frames >= confirm_frames && !state.alerted) {
            state.alerted = true;

            std::string label = cvedix_utils::string_format(
                "FALL DETECTED (ratio=%.2f, angle=%.1f°)", ratio, angle);

            std::vector<int> involve_targets;
            // Try to match pose to a tracked target by bbox overlap
            for (const auto& target : meta->targets) {
                if (target->track_id < 0) continue;
                // Simple overlap check
                bool overlap_x = (target->x < bx + bw) && (target->x + target->width > bx);
                bool overlap_y = (target->y < by + bh) && (target->y + target->height > by);
                if (overlap_x && overlap_y) {
                    involve_targets.push_back(target->track_id);
                    break;
                }
            }

            std::vector<cvedix_objects::cvedix_point> involve_region = {
                cvedix_objects::cvedix_point(bx, by),
                cvedix_objects::cvedix_point(bx + bw, by),
                cvedix_objects::cvedix_point(bx + bw, by + bh),
                cvedix_objects::cvedix_point(bx, by + bh)
            };

            std::string image_file = "";
            std::string video_file = "";

            if (need_record_image) {
                image_file = cvedix_utils::time_format(
                    NOW, "fall_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_image_record_control_meta>(
                    ch, image_file, true));
            }

            if (need_record_video) {
                video_file = cvedix_utils::time_format(
                    NOW, "fall_ch" + std::to_string(ch) +
                             "__<year><mon><day><hour><min><sec><mili>");
                pendding_meta(std::make_shared<
                    cvedix_objects::cvedix_video_record_control_meta>(
                    ch, video_file));
            }

            auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                cvedix_objects::cvedix_ba_type::FALL, ch,
                meta->frame_index, involve_targets, involve_region, label,
                image_file, video_file);
            meta->ba_results.push_back(ba_result);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] pose %zu: %s",
                node_name.c_str(), ch, pi, label.c_str()));
        }
    }

    // Prune old pose states if pose count changed
    if (pose_states.size() > meta->pose_targets.size() * 2) {
        for (auto it = pose_states.begin(); it != pose_states.end();) {
            if (it->first >= static_cast<int>(meta->pose_targets.size())) {
                it = pose_states.erase(it);
            } else {
                ++it;
            }
        }
    }

    return meta;
}

} // namespace cvedix_nodes
