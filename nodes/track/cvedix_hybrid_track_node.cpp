/**
 * @file cvedix_hybrid_track_node.cpp
 * @brief Hybrid tracker: ByteTrack + OpenCV KCF
 *
 * Improved approach:
 *   1. Update KCF trackers for all active tracks using current frame
 *   2. For lost tracks (KCF active but no detection), add KCF-predicted 
 *      bbox as a synthetic detection input to ByteTrack
 *   3. Run ByteTrack with combined real + synthetic detections
 *   4. ByteTrack handles association, deduplication, and ID management
 *   5. Init/reset KCF for matched detections
 * 
 * This avoids double-counting by letting ByteTrack be the single source 
 * of truth for track ID assignment.
 */

#include "cvedix_hybrid_track_node.h"

#include <algorithm>
#include <set>
#include <opencv2/core.hpp>
#include <opencv2/tracking.hpp>

namespace cvedix_nodes {

// ============================
// Constructor
// ============================
cvedix_hybrid_track_node::cvedix_hybrid_track_node(
    std::string node_name,
    cvedix_track_for track_for,
    float track_thresh,
    float high_thresh,
    float match_thresh,
    int track_buffer,
    int frame_rate,
    int kcf_max_frames)
    : cvedix_track_node(std::move(node_name), track_for),
      m_track_thresh(track_thresh),
      m_high_thresh(high_thresh),
      m_match_thresh(match_thresh),
      m_track_buffer(track_buffer),
      m_fps(frame_rate),
      m_kcf_max_frames(kcf_max_frames)
{
    this->initialized();
}

cvedix_hybrid_track_node::~cvedix_hybrid_track_node() {
    deinitialized();
}

// ============================
// IoU helper
// ============================
float cvedix_hybrid_track_node::iou_tlwh(
    const cv::Rect2f& a,
    const cv::Rect2f& b)
{
    float xx1 = std::max(a.x, b.x);
    float yy1 = std::max(a.y, b.y);
    float xx2 = std::min(a.x + a.width,  b.x + b.width);
    float yy2 = std::min(a.y + a.height, b.y + b.height);

    float w = std::max(0.f, xx2 - xx1);
    float h = std::max(0.f, yy2 - yy1);
    float inter = w * h;
    float uni = a.width * a.height + b.width * b.height - inter;

    return (uni > 0.f) ? inter / uni : 0.f;
}

// ============================
// track() — HYBRID logic v2
// ============================
void cvedix_hybrid_track_node::track(
    int channel_index,
    const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta,
    const std::vector<cvedix_objects::cvedix_rect>& target_rects,
    const std::vector<float>& target_scores,
    const std::vector<std::vector<float>>& target_embeddings,
    std::vector<int>& track_ids)
{
    track_ids.assign(target_rects.size(), -1);

    if (channel_index < 0) return;

    // --- Init ByteTrack per channel ---
    if ((size_t)channel_index >= m_channel_trackers.size()) {
        m_channel_trackers.resize(channel_index + 1);
    }

    auto& ctx = m_channel_trackers[channel_index];
    if (!ctx.initialized) {
        CVEDIX_INFO(cvedix_utils::string_format("[%s] Init hybrid tracker (ByteTrack + KCF)", node_name.c_str()));
        ctx.tracker = std::make_unique<BYTETracker>(m_fps, m_track_buffer);
        ctx.initialized = true;
    }

    const cv::Mat& frame = frame_meta->frame;
    auto& kcf_states = m_kcf_states[channel_index];

    // ========================
    // 1) Build detection set from real detections
    // ========================
    size_t n_real = target_rects.size();
    std::vector<Object> objects;
    objects.reserve(n_real + kcf_states.size());

    for (size_t i = 0; i < n_real; ++i) {
        const auto& r = target_rects[i];
        float score = (i < target_scores.size()) ? target_scores[i] : 1.0f;
        Object obj;
        obj.rect = cv::Rect2f(
            static_cast<float>(r.x), static_cast<float>(r.y),
            static_cast<float>(r.width), static_cast<float>(r.height)
        );
        obj.prob = score;
        obj.label = 0;
        objects.push_back(obj);
    }

    // ========================
    // 2) Update KCF for lost tracks → add as synthetic detections  
    // ========================
    // Check which real detections overlap with existing KCF tracks
    std::set<int> real_detection_tids;  // track IDs covered by real detections (approx)
    
    std::vector<int> kcf_to_remove;

    for (auto& [tid, kcf] : kcf_states) {
        if (!kcf.active) continue;

        // Check if a real detection overlaps this KCF bbox
        bool has_real_detection = false;
        for (size_t i = 0; i < n_real; ++i) {
            cv::Rect2f det_box(
                static_cast<float>(target_rects[i].x),
                static_cast<float>(target_rects[i].y),
                static_cast<float>(target_rects[i].width),
                static_cast<float>(target_rects[i].height)
            );
            cv::Rect2f kcf_box(
                static_cast<float>(kcf.bbox.x),
                static_cast<float>(kcf.bbox.y),
                static_cast<float>(kcf.bbox.width),
                static_cast<float>(kcf.bbox.height)
            );
            if (iou_tlwh(det_box, kcf_box) > 0.3f) {
                has_real_detection = true;
                break;
            }
        }

        if (has_real_detection) {
            // Real detection covers this track → KCF is backup only, reset counter
            kcf.frames_without_det = 0;
            // Don't add synthetic detection, real one is enough
            // KCF will be re-initialized below when ByteTrack matches
            continue;
        }

        // No real detection → use KCF to predict
        kcf.frames_without_det++;

        if (m_kcf_max_frames > 0 && kcf.frames_without_det > m_kcf_max_frames) {
            kcf.active = false;
            kcf_to_remove.push_back(tid);
            continue;
        }

        // Update KCF
        if (!frame.empty() && kcf.tracker) {
            bool ok = kcf.tracker->update(frame, kcf.bbox);
            if (ok && kcf.bbox.width > 5 && kcf.bbox.height > 5 &&
                kcf.bbox.x >= 0 && kcf.bbox.y >= 0 &&
                kcf.bbox.x + kcf.bbox.width <= frame.cols &&
                kcf.bbox.y + kcf.bbox.height <= frame.rows) {

                // Add KCF prediction as a synthetic detection for ByteTrack
                // Use moderate confidence so ByteTrack treats it as a low-conf detection
                Object synth;
                synth.rect = cv::Rect2f(
                    static_cast<float>(kcf.bbox.x),
                    static_cast<float>(kcf.bbox.y),
                    static_cast<float>(kcf.bbox.width),
                    static_cast<float>(kcf.bbox.height)
                );
                synth.prob = 0.3f;  // moderate score → ByteTrack's 2nd stage matching
                synth.label = 0;
                objects.push_back(synth);
            } else {
                // KCF failed
                kcf.active = false;
                kcf_to_remove.push_back(tid);
            }
        }
    }

    // Cleanup dead KCF trackers
    for (int tid : kcf_to_remove) {
        kcf_states.erase(tid);
    }

    // ========================
    // 3) ByteTrack update with real + synthetic detections
    // ========================
    std::vector<STrack> bt_tracks = ctx.tracker->update(objects);

    // ========================
    // 4) Assign track IDs to REAL detections only
    // ========================
    std::vector<cv::Rect2f> track_boxes;
    std::vector<int> bt_track_ids;
    track_boxes.reserve(bt_tracks.size());
    bt_track_ids.reserve(bt_tracks.size());

    for (const auto& t : bt_tracks) {
        track_boxes.emplace_back(t.tlwh[0], t.tlwh[1], t.tlwh[2], t.tlwh[3]);
        bt_track_ids.push_back(t.track_id);
    }

    const float IOU_ASSIGN_THRESH = 0.3f;
    std::vector<bool> used(bt_tracks.size(), false);
    std::set<int> matched_tids;

    for (size_t i = 0; i < n_real; ++i) {
        cv::Rect2f in_box(
            static_cast<float>(target_rects[i].x),
            static_cast<float>(target_rects[i].y),
            static_cast<float>(target_rects[i].width),
            static_cast<float>(target_rects[i].height)
        );

        float best_iou = 0.f;
        int best_j = -1;

        for (size_t j = 0; j < bt_tracks.size(); ++j) {
            if (used[j]) continue;
            float iou = iou_tlwh(in_box, track_boxes[j]);
            if (iou > best_iou) {
                best_iou = iou;
                best_j = static_cast<int>(j);
            }
        }

        if (best_j >= 0 && best_iou >= IOU_ASSIGN_THRESH) {
            int tid = bt_track_ids[best_j];
            track_ids[i] = tid;
            used[best_j] = true;
            matched_tids.insert(tid);

            // Init/reset KCF for matched detection
            cv::Rect roi(
                target_rects[i].x, target_rects[i].y,
                target_rects[i].width, target_rects[i].height
            );
            roi.x = std::max(0, roi.x);
            roi.y = std::max(0, roi.y);
            roi.width = std::min(roi.width, frame.cols - roi.x);
            roi.height = std::min(roi.height, frame.rows - roi.y);

            if (roi.width > 10 && roi.height > 10 && !frame.empty()) {
                auto& kcf = kcf_states[tid];
                kcf.tracker = cv::TrackerKCF::create();
                kcf.tracker->init(frame, roi);
                kcf.bbox = roi;
                kcf.track_id = tid;
                kcf.frames_without_det = 0;
                kcf.active = true;
            }
        }
    }
}

} // namespace cvedix_nodes
