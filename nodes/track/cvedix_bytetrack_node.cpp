#include "cvedix_bytetrack_node.h"
 
#include <algorithm>
#include <opencv2/core.hpp>
 
namespace cvedix_nodes {
 
// ============================
// Constructor
// ============================
cvedix_bytetrack_node::cvedix_bytetrack_node(
    std::string node_name,
    cvedix_track_for track_for,
    float track_thresh,
    float high_thresh,
    float match_thresh,
    int track_buffer,
    int frame_rate)
    : cvedix_track_node(std::move(node_name), track_for),
      m_track_thresh(track_thresh),
      m_high_thresh(high_thresh),
      m_match_thresh(match_thresh),
      m_track_buffer(track_buffer),
      m_fps(frame_rate)
{
    this->initialized();
}

cvedix_bytetrack_node::~cvedix_bytetrack_node() 
{
    deinitialized();
}
 
// ============================
// IoU helper (TLWH)
// ============================
float cvedix_bytetrack_node::iou_tlwh(
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
// track() — CORE ByteTrack logic
// ============================
void cvedix_bytetrack_node::track(
    int channel_index,
    const std::vector<cvedix_objects::cvedix_rect>& target_rects,
    const std::vector<std::vector<float>>& /*target_embeddings*/,
    std::vector<int>& track_ids)
{
    // Base class expects same order & size
    track_ids.assign(target_rects.size(), -1);
    
    if (channel_index < 0) {
        return;
    }
 
    // One tracker per channel
    if ((size_t)channel_index >= m_channel_trackers.size()) {
        m_channel_trackers.resize(channel_index + 1);
    }
 
    auto& ctx = m_channel_trackers[channel_index];
 
    if (!ctx.initialized) {
        CVEDIX_INFO(cvedix_utils::string_format("[%s] Init bytetrack", node_name.c_str()));
        ctx.tracker = std::make_unique<BYTETracker>(
            m_fps,
            m_track_buffer
        );
        ctx.initialized = true;
    }
 
    // ========================
    // 1) Convert rects → Objects
    // ========================
    std::vector<Object> objects;
    objects.reserve(target_rects.size());
 
    for (const auto& r : target_rects) {
        Object obj;
        obj.rect  = cv::Rect2f(
            static_cast<float>(r.x),
            static_cast<float>(r.y),
            static_cast<float>(r.width),
            static_cast<float>(r.height)
        );
        obj.prob  = 1.0f;  // ⚠ no score in cvedix_rect
        obj.label = 0;
        objects.push_back(obj);
    }
 
    // ========================
    // 2) ByteTrack update
    // ========================
    std::vector<STrack> tracks = ctx.tracker->update(objects);
 
    // ========================
    // 3) Prepare track boxes
    // ========================
    std::vector<cv::Rect2f> track_boxes;
    track_boxes.reserve(tracks.size());
 
    for (const auto& t : tracks) {
        track_boxes.emplace_back(
            t.tlwh[0],
            t.tlwh[1],
            t.tlwh[2],
            t.tlwh[3]
        );
    }
 
    // ========================
    // 4) Assign track_id by IoU
    // ========================
    const float IOU_ASSIGN_THRESH = 0.3f;
    std::vector<bool> used(tracks.size(), false);
 
    for (size_t i = 0; i < target_rects.size(); ++i) {
        cv::Rect2f in_box(
            static_cast<float>(target_rects[i].x),
            static_cast<float>(target_rects[i].y),
            static_cast<float>(target_rects[i].width),
            static_cast<float>(target_rects[i].height)
        );
 
        float best_iou = 0.f;
        int best_j = -1;
 
        for (size_t j = 0; j < tracks.size(); ++j) {
            if (used[j]) continue;
 
            float iou = iou_tlwh(in_box, track_boxes[j]);
            if (iou > best_iou) {
                best_iou = iou;
                best_j = static_cast<int>(j);
            }
        }
 
        if (best_j >= 0 && best_iou >= IOU_ASSIGN_THRESH) {
            track_ids[i] = tracks[best_j].track_id;
            used[best_j] = true;
        }
    }
}
 
} // namespace cvedix_nodes