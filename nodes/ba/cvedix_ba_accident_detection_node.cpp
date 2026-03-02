/**
 * @file cvedix_ba_accident_detection_node.cpp
 * @brief Traffic accident detection implementation
 *
 * Detection methods:
 * 1. COLLISION: Two tracked objects overlap (IoU > threshold)
 *    while both were previously moving
 * 2. SUDDEN_STOP: Object was moving > min_speed, now stopped
 * 3. SWERVE: Object trajectory changes direction > threshold degrees
 */

#include "cvedix_ba_accident_detection_node.h"

#include <cmath>
#include <algorithm>

namespace cvedix_nodes {

// ============================
// Constructor
// ============================
cvedix_ba_accident_detection_node::cvedix_ba_accident_detection_node(
    std::string node_name,
    float collision_iou,
    float min_speed,
    float direction_thresh,
    int history_frames)
    : cvedix_node(node_name),
      collision_iou_thresh(collision_iou),
      min_moving_speed(min_speed),
      direction_change_thresh(direction_thresh),
      history_frames(history_frames)
{
    CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(),
                                            to_string().c_str()));
    this->initialized();
}

cvedix_ba_accident_detection_node::~cvedix_ba_accident_detection_node() {
    deinitialized();
}

std::string cvedix_ba_accident_detection_node::to_string() {
    return cvedix_utils::string_format(
        "accident_detection(collision_iou=%.2f, min_speed=%.1f, dir_thresh=%.0f°, history=%d)",
        collision_iou_thresh, min_moving_speed, direction_change_thresh, history_frames);
}

// ============================
// IoU helper
// ============================
float cvedix_ba_accident_detection_node::calculate_iou(
    int x1, int y1, int w1, int h1,
    int x2, int y2, int w2, int h2)
{
    int xx1 = std::max(x1, x2);
    int yy1 = std::max(y1, y2);
    int xx2 = std::min(x1 + w1, x2 + w2);
    int yy2 = std::min(y1 + h1, y2 + h2);

    int iw = std::max(0, xx2 - xx1);
    int ih = std::max(0, yy2 - yy1);
    float inter = static_cast<float>(iw * ih);
    float area1 = static_cast<float>(w1 * h1);
    float area2 = static_cast<float>(w2 * h2);
    float uni = area1 + area2 - inter;

    return (uni > 0.f) ? inter / uni : 0.f;
}

// ============================
// Average speed (px/frame)
// ============================
float cvedix_ba_accident_detection_node::calculate_avg_speed(
    const std::deque<std::pair<int, int>>& history)
{
    if (history.size() < 2) return 0.f;

    float total_dist = 0.f;
    size_t count = 0;
    size_t start = (history.size() > 5) ? history.size() - 5 : 0;
    
    for (size_t i = start; i < history.size() - 1; ++i) {
        float dx = static_cast<float>(history[i + 1].first - history[i].first);
        float dy = static_cast<float>(history[i + 1].second - history[i].second);
        total_dist += std::sqrt(dx * dx + dy * dy);
        count++;
    }

    return (count > 0) ? total_dist / count : 0.f;
}

// ============================
// Direction change (degrees)
// ============================
float cvedix_ba_accident_detection_node::calculate_direction_change(
    const std::deque<std::pair<int, int>>& history)
{
    if (history.size() < 6) return 0.f;

    // Direction from first half
    size_t mid = history.size() / 2;
    float dx1 = static_cast<float>(history[mid].first - history[0].first);
    float dy1 = static_cast<float>(history[mid].second - history[0].second);
    
    // Direction from second half
    float dx2 = static_cast<float>(history.back().first - history[mid].first);
    float dy2 = static_cast<float>(history.back().second - history[mid].second);

    float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
    float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

    if (len1 < 2.0f || len2 < 2.0f) return 0.f;

    // Cosine of angle between two direction vectors
    float dot = (dx1 * dx2 + dy1 * dy2) / (len1 * len2);
    dot = std::max(-1.0f, std::min(1.0f, dot));

    return std::acos(dot) * 180.0f / 3.14159265f;
}

// ============================
// handle_frame_meta — main logic
// ============================
std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_ba_accident_detection_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta)
{
    std::lock_guard<std::mutex> lock(config_mutex);
    int ch = meta->channel_index;

    // ========================
    // 1) Update track history
    // ========================
    auto& ch_history = track_history[ch];
    std::set<int> active_tids;

    for (auto& target : meta->targets) {
        if (target->track_id < 0) continue;
        int tid = target->track_id;
        active_tids.insert(tid);

        int cx = target->x + target->width / 2;
        int cy = target->y + target->height / 2;

        ch_history[tid].push_back({cx, cy});
        if ((int)ch_history[tid].size() > history_frames) {
            ch_history[tid].pop_front();
        }
    }

    // Cleanup old histories
    for (auto it = ch_history.begin(); it != ch_history.end();) {
        if (active_tids.count(it->first) == 0) {
            alerted_stops[ch].erase(it->first);
            accident_labels[ch].erase(it->first);
            it = ch_history.erase(it);
        } else {
            ++it;
        }
    }

    // ========================
    // 2) Collision detection
    // ========================
    auto& targets = meta->targets;
    for (size_t i = 0; i < targets.size(); ++i) {
        if (targets[i]->track_id < 0) continue;
        for (size_t j = i + 1; j < targets.size(); ++j) {
            if (targets[j]->track_id < 0) continue;

            // === Skip rider-vehicle pairs ===
            // COCO: 0=person, 1=bicycle, 3=motorcycle
            int cls_i = targets[i]->primary_class_id;
            int cls_j = targets[j]->primary_class_id;

            // Skip person↔motorcycle, person↔bicycle (rider on vehicle)
            bool is_rider_pair = 
                (cls_i == 0 && (cls_j == 1 || cls_j == 3)) ||
                (cls_j == 0 && (cls_i == 1 || cls_i == 3));
            if (is_rider_pair) continue;

            // Skip person↔person overlap (not vehicle collision)
            if (cls_i == 0 && cls_j == 0) continue;

            float iou = calculate_iou(
                targets[i]->x, targets[i]->y, targets[i]->width, targets[i]->height,
                targets[j]->x, targets[j]->y, targets[j]->width, targets[j]->height
            );

            if (iou >= collision_iou_thresh) {
                int tid1 = std::min(targets[i]->track_id, targets[j]->track_id);
                int tid2 = std::max(targets[i]->track_id, targets[j]->track_id);
                auto pair_key = std::make_pair(tid1, tid2);

                if (alerted_collisions[ch].count(pair_key) > 0) continue;

                // Check if both were moving
                float speed1 = calculate_avg_speed(ch_history[tid1]);
                float speed2 = calculate_avg_speed(ch_history[tid2]);

                if (speed1 > min_moving_speed || speed2 > min_moving_speed) {
                    // COLLISION DETECTED!
                    std::string label = cvedix_utils::string_format(
                        "⚠ COLLISION #%d↔#%d (IoU=%.0f%%)",
                        tid1, tid2, iou * 100);

                    CVEDIX_INFO(cvedix_utils::string_format(
                        "[%s] [ch%d] %s", node_name.c_str(), ch, label.c_str()));

                    std::vector<int> involved = {tid1, tid2};
                    auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(
                        cvedix_objects::cvedix_ba_type::CROSSLINE, ch,
                        meta->frame_index, involved,
                        std::vector<cvedix_objects::cvedix_point>{}, label, "", "");
                    meta->ba_results.push_back(ba_result);

                    // Add labels to both targets
                    std::string collision_label = cvedix_utils::string_format(
                        "[COLLISION] IoU=%.0f%%", iou * 100);
                    targets[i]->secondary_labels.push_back(collision_label);
                    targets[j]->secondary_labels.push_back(collision_label);
                    accident_labels[ch][tid1] = collision_label;
                    accident_labels[ch][tid2] = collision_label;

                    alerted_collisions[ch].insert(pair_key);
                }
            }
        }
    }

    // ========================
    // 3) Sudden stop detection
    // ========================
    for (auto& target : targets) {
        if (target->track_id < 0) continue;
        int tid = target->track_id;
        auto& hist = ch_history[tid];

        if ((int)hist.size() < history_frames) continue;
        if (alerted_stops[ch].count(tid) > 0) continue;

        // Check recent speed vs older speed
        float current_speed = 0.f;
        float past_speed = 0.f;

        // Current speed (last 3 frames)
        if (hist.size() >= 3) {
            auto recent_start = hist.end() - 3;
            std::deque<std::pair<int, int>> recent(recent_start, hist.end());
            current_speed = calculate_avg_speed(recent);
        }

        // Past speed (frames 0 to mid)
        if (hist.size() >= 6) {
            size_t mid = hist.size() / 2;
            std::deque<std::pair<int, int>> past(hist.begin(), hist.begin() + mid);
            past_speed = calculate_avg_speed(past);
        }

        // Sudden stop: was moving fast, now barely moving
        if (past_speed > min_moving_speed * 2 && current_speed < min_moving_speed * 0.3f) {
            std::string label = cvedix_utils::string_format(
                "⚠ SUDDEN STOP #%d (%.1f→%.1f px/f)", tid, past_speed, current_speed);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] %s", node_name.c_str(), ch, label.c_str()));

            std::string stop_label = "[SUDDEN STOP]";
            target->secondary_labels.push_back(stop_label);
            accident_labels[ch][tid] = stop_label;

            alerted_stops[ch].insert(tid);
        }
    }

    // ========================
    // 4) Direction change detection (swerve)
    // ========================
    for (auto& target : targets) {
        if (target->track_id < 0) continue;
        int tid = target->track_id;
        auto& hist = ch_history[tid];

        if ((int)hist.size() < history_frames) continue;

        float dir_change = calculate_direction_change(hist);
        if (dir_change > direction_change_thresh) {
            // Only alert once per track
            std::string key = "swerve_" + std::to_string(tid);
            if (accident_labels[ch].count(tid) > 0 &&
                accident_labels[ch][tid].find("SWERVE") != std::string::npos) continue;

            std::string label = cvedix_utils::string_format(
                "⚠ SWERVE #%d (%.0f°)", tid, dir_change);

            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] [ch%d] %s", node_name.c_str(), ch, label.c_str()));

            std::string swerve_label = cvedix_utils::string_format(
                "[SWERVE %.0f°]", dir_change);
            target->secondary_labels.push_back(swerve_label);
            accident_labels[ch][tid] = swerve_label;
        }
    }

    // ========================
    // 5) Persist accident labels on targets
    // ========================
    for (auto& target : targets) {
        if (target->track_id < 0) continue;
        int tid = target->track_id;
        if (accident_labels[ch].count(tid) > 0 && target->secondary_labels.empty()) {
            target->secondary_labels.push_back(accident_labels[ch][tid]);
        }
    }

    return meta;
}

} // namespace cvedix_nodes
