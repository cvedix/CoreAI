/**
 * @file cvedix_hybrid_track_node.h
 * @brief Hybrid tracker: ByteTrack + OpenCV KCF for persistent tracking
 * 
 * Combines ByteTrack's detection-based association with OpenCV's KCF 
 * correlation tracker for "detect once, track always" behavior.
 * 
 * When ByteTrack loses a detection, KCF maintains the track using
 * visual features from the raw frame, providing much more robust 
 * tracking than Kalman prediction alone.
 * 
 * @section hybrid_how How it works:
 * 1. ByteTrack associates detections → track IDs (primary)
 * 2. Lost tracks (no detection match) → KCF predicts bbox from frame
 * 3. KCF-predicted bboxes are injected back as "virtual detections"  
 * 4. Track only dies when BOTH ByteTrack AND KCF fail
 */

#pragma once

#include "cvedix_track_node.h"
#include "bytetrack/BYTETracker.h"
#include "bytetrack/STrack.h"
#include "bytetrack/dataType.h"

#include <opencv2/tracking.hpp>
#include <map>
#include <memory>

namespace cvedix_nodes {

class cvedix_hybrid_track_node final : public cvedix_track_node {
public:
    /**
     * @brief Constructor
     * @param node_name Unique node identifier
     * @param track_for Target type (NORMAL or FACE)
     * @param track_thresh ByteTrack: minimum detection score to track
     * @param high_thresh ByteTrack: high/low confidence split
     * @param match_thresh ByteTrack: IoU matching threshold
     * @param track_buffer ByteTrack: frames to keep lost tracks
     * @param frame_rate Video FPS for Kalman filter
     * @param kcf_max_frames Max frames KCF can track without detection (0=unlimited)
     */
    cvedix_hybrid_track_node(
        std::string node_name,
        cvedix_track_for track_for,
        float track_thresh = 0.1f,
        float high_thresh = 0.5f,
        float match_thresh = 0.7f,
        int track_buffer = 90,
        int frame_rate = 25,
        int kcf_max_frames = 150
    );

    virtual ~cvedix_hybrid_track_node();

protected:
    virtual void track(int channel_index,
                       const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta,
                       const std::vector<cvedix_objects::cvedix_rect>& target_rects,
                       const std::vector<float>& target_scores,
                       const std::vector<std::vector<float>>& target_embeddings,
                       std::vector<int>& track_ids) override;

private:
    // ByteTrack params
    float m_track_thresh;
    float m_high_thresh;
    float m_match_thresh;
    int   m_track_buffer;
    int   m_fps;
    int   m_kcf_max_frames;

    // Per-channel ByteTrack
    struct ChannelTracker {
        std::unique_ptr<BYTETracker> tracker;
        bool initialized = false;
    };
    std::vector<ChannelTracker> m_channel_trackers;

    // Per-track KCF state
    struct KCFTrackState {
        cv::Ptr<cv::Tracker> tracker;
        cv::Rect bbox;
        int track_id;
        int frames_without_det = 0;  // frames tracked by KCF only
        bool active = true;
    };

    // channel → track_id → KCF state
    std::map<int, std::map<int, KCFTrackState>> m_kcf_states;

    static float iou_tlwh(const cv::Rect2f& a, const cv::Rect2f& b);
};

} // namespace cvedix_nodes
