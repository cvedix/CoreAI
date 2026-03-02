#include "cvedix_ba_fight_detection_node.h"

namespace cvedix_nodes {

cvedix_ba_fight_detection_node::cvedix_ba_fight_detection_node(
    std::string node_name,
    float proximity_threshold,
    float arm_speed_threshold,
    float fight_score_threshold,
    int confirm_frames,
    bool need_record_image,
    bool need_record_video)
    : cvedix_node(node_name),
      proximity_threshold(proximity_threshold),
      arm_speed_threshold(arm_speed_threshold),
      fight_score_threshold(fight_score_threshold),
      confirm_frames(confirm_frames),
      need_record_image(need_record_image),
      need_record_video(need_record_video) {
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] fight_detection(proximity=%.0f, arm_speed=%.0f, score_thr=%.2f, confirm=%d)",
        node_name.c_str(), proximity_threshold, arm_speed_threshold,
        fight_score_threshold, confirm_frames));
    this->initialized();
}

cvedix_ba_fight_detection_node::~cvedix_ba_fight_detection_node() {
    deinitialized();
}

std::string cvedix_ba_fight_detection_node::to_string() {
    return cvedix_utils::string_format(
        "fight_detection(proximity=%.0f, arm_speed=%.0f, score=%.2f, confirm=%d)",
        proximity_threshold, arm_speed_threshold,
        fight_score_threshold, confirm_frames);
}

std::pair<float, float> cvedix_ba_fight_detection_node::pose_center(
    const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps) const {
    // Use hips (11, 12) as center; fallback to all keypoints average
    float cx = 0, cy = 0;
    int count = 0;
    float min_score = 0.3f;

    // Try hips first
    for (const auto& kp : kps) {
        if (kp.score < min_score) continue;
        if (kp.point_type == 11 || kp.point_type == 12) {
            cx += kp.x;
            cy += kp.y;
            count++;
        }
    }

    if (count > 0) return {cx / count, cy / count};

    // Fallback: average all valid keypoints
    for (const auto& kp : kps) {
        if (kp.score < min_score) continue;
        cx += kp.x;
        cy += kp.y;
        count++;
    }

    if (count > 0) return {cx / count, cy / count};
    return {0, 0};
}

float cvedix_ba_fight_detection_node::calculate_fight_score(
    const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps1,
    const std::vector<cvedix_objects::cvedix_pose_keypoint>& kps2,
    int idx1, int idx2) const {
    float score = 0.0f;
    float min_score = 0.3f;

    // --- Factor 1: Arm speed (rapid movement) ---
    // Check wrist displacement from previous frame for both persons
    float arm_motion_score = 0.0f;
    int arm_checks = 0;

    for (int idx : {idx1, idx2}) {
        if (prev_wrists.count(idx) == 0) continue;

        const auto& [plw_x, plw_y, prw_x, prw_y] = prev_wrists.at(idx);
        const auto& kps = (idx == idx1) ? kps1 : kps2;

        for (const auto& kp : kps) {
            if (kp.score < min_score) continue;
            if (kp.point_type == 9) {  // left wrist
                float dx = kp.x - plw_x;
                float dy = kp.y - plw_y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > arm_speed_threshold) arm_motion_score += 0.5f;
                arm_checks++;
            } else if (kp.point_type == 10) {  // right wrist
                float dx = kp.x - prw_x;
                float dy = kp.y - prw_y;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > arm_speed_threshold) arm_motion_score += 0.5f;
                arm_checks++;
            }
        }
    }

    if (arm_checks > 0) {
        score += std::min(arm_motion_score, 0.4f);  // Max 0.4 from arm speed
    }

    // --- Factor 2: Raised arms (wrist above shoulder) ---
    float raised_score = 0.0f;
    for (const auto* kps_ptr : {&kps1, &kps2}) {
        const auto& kps = *kps_ptr;
        float shoulder_y = 0;
        int shoulder_count = 0;

        for (const auto& kp : kps) {
            if (kp.score < min_score) continue;
            if (kp.point_type == 5 || kp.point_type == 6) {
                shoulder_y += kp.y;
                shoulder_count++;
            }
        }

        if (shoulder_count > 0) {
            shoulder_y /= shoulder_count;
            for (const auto& kp : kps) {
                if (kp.score < min_score) continue;
                if (kp.point_type == 9 || kp.point_type == 10) {
                    // Wrist above shoulder (y decreases upward)
                    if (kp.y < shoulder_y - 20) {
                        raised_score += 0.15f;
                    }
                }
            }
        }
    }
    score += std::min(raised_score, 0.3f);  // Max 0.3 from raised arms

    // --- Factor 3: Overlapping/interleaved keypoints ---
    // If limbs of person1 are within person2's bounding area
    auto [cx1, cy1] = pose_center(kps1);
    auto [cx2, cy2] = pose_center(kps2);
    float center_dist = std::sqrt((cx1 - cx2) * (cx1 - cx2) + (cy1 - cy2) * (cy1 - cy2));

    if (center_dist < proximity_threshold * 0.5f) {
        score += 0.3f;  // Very close = more likely fighting
    } else if (center_dist < proximity_threshold * 0.75f) {
        score += 0.15f;
    }

    return std::min(score, 1.0f);
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_fight_detection_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::lock_guard<std::mutex> lock(config_mutex);

    auto ch = meta->channel_index;
    auto& poses = meta->pose_targets;

    if (poses.size() < 2) {
        // Need at least 2 people
        pair_states.clear();
        goto update_wrists;
    }

    {
        std::set<std::string> active_pairs;

        // Check all pairs
        for (size_t i = 0; i < poses.size(); i++) {
            for (size_t j = i + 1; j < poses.size(); j++) {
                auto [cx1, cy1] = pose_center(poses[i]->key_points);
                auto [cx2, cy2] = pose_center(poses[j]->key_points);

                float dist = std::sqrt((cx1 - cx2) * (cx1 - cx2) +
                                       (cy1 - cy2) * (cy1 - cy2));

                // Skip if too far apart
                if (dist > proximity_threshold) continue;

                std::string pair_key = std::to_string(i) + "_" + std::to_string(j);
                active_pairs.insert(pair_key);

                float fight_score = calculate_fight_score(
                    poses[i]->key_points, poses[j]->key_points,
                    static_cast<int>(i), static_cast<int>(j));

                auto& state = pair_states[pair_key];

                if (fight_score >= fight_score_threshold) {
                    state.consecutive_frames++;
                } else {
                    state.consecutive_frames = std::max(0, state.consecutive_frames - 1);
                    if (state.consecutive_frames == 0) state.alerted = false;
                }

                // Confirm fight
                if (state.consecutive_frames >= confirm_frames && !state.alerted) {
                    state.alerted = true;

                    std::string label = cvedix_utils::string_format(
                        "FIGHT DETECTED (score=%.2f, persons %zu & %zu)",
                        fight_score, i, j);

                    // Build involve region from both poses' bounding area
                    int min_x = INT_MAX, min_y = INT_MAX, max_x = 0, max_y = 0;
                    for (const auto* kps_ptr : {&poses[i]->key_points, &poses[j]->key_points}) {
                        for (const auto& kp : *kps_ptr) {
                            if (kp.score < 0.3f) continue;
                            if (kp.x < min_x) min_x = kp.x;
                            if (kp.y < min_y) min_y = kp.y;
                            if (kp.x > max_x) max_x = kp.x;
                            if (kp.y > max_y) max_y = kp.y;
                        }
                    }

                    std::vector<cvedix_objects::cvedix_point> involve_region = {
                        cvedix_objects::cvedix_point(min_x, min_y),
                        cvedix_objects::cvedix_point(max_x, min_y),
                        cvedix_objects::cvedix_point(max_x, max_y),
                        cvedix_objects::cvedix_point(min_x, max_y)
                    };

                    // Match poses to tracked targets
                    std::vector<int> involve_targets;
                    for (const auto& target : meta->targets) {
                        if (target->track_id < 0) continue;
                        bool overlap_x = (target->x < max_x) && (target->x + target->width > min_x);
                        bool overlap_y = (target->y < max_y) && (target->y + target->height > min_y);
                        if (overlap_x && overlap_y) {
                            involve_targets.push_back(target->track_id);
                        }
                    }

                    std::string image_file = "";
                    std::string video_file = "";

                    if (need_record_image) {
                        image_file = cvedix_utils::time_format(
                            NOW, "fight_ch" + std::to_string(ch) +
                                     "__<year><mon><day><hour><min><sec><mili>");
                        pendding_meta(std::make_shared<
                            cvedix_objects::cvedix_image_record_control_meta>(
                            ch, image_file, true));
                    }

                    if (need_record_video) {
                        video_file = cvedix_utils::time_format(
                            NOW, "fight_ch" + std::to_string(ch) +
                                     "__<year><mon><day><hour><min><sec><mili>");
                        pendding_meta(std::make_shared<
                            cvedix_objects::cvedix_video_record_control_meta>(
                            ch, video_file));
                    }

                    auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                        cvedix_objects::cvedix_ba_type::FIGHT, ch,
                        meta->frame_index, involve_targets, involve_region, label,
                        image_file, video_file);
                    meta->ba_results.push_back(ba_result);

                    CVEDIX_INFO(cvedix_utils::string_format(
                        "[%s] [ch%d] %s", node_name.c_str(), ch, label.c_str()));
                }
            }
        }

        // Prune inactive pairs
        for (auto it = pair_states.begin(); it != pair_states.end();) {
            if (active_pairs.count(it->first) == 0) {
                it = pair_states.erase(it);
            } else {
                ++it;
            }
        }
    }

update_wrists:
    // Save current wrist positions for next frame
    prev_wrists.clear();
    for (size_t i = 0; i < poses.size(); i++) {
        float lw_x = 0, lw_y = 0, rw_x = 0, rw_y = 0;
        for (const auto& kp : poses[i]->key_points) {
            if (kp.score < 0.3f) continue;
            if (kp.point_type == 9) { lw_x = kp.x; lw_y = kp.y; }
            if (kp.point_type == 10) { rw_x = kp.x; rw_y = kp.y; }
        }
        prev_wrists[static_cast<int>(i)] = {lw_x, lw_y, rw_x, rw_y};
    }

    return meta;
}

} // namespace cvedix_nodes
