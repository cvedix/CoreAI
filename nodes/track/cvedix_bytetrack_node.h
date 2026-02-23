#pragma once
 
#include <memory>
#include <vector>
 
#include "cvedix_track_node.h"
 

#include "bytetrack/BYTETracker.h"
#include "bytetrack/STrack.h"
#include "bytetrack/dataType.h"
 
namespace cvedix_nodes {
 
class cvedix_bytetrack_node final : public cvedix_track_node {
public:
    cvedix_bytetrack_node(
        std::string node_name,
        cvedix_track_for track_for,
        float track_thresh,
        float high_thresh,
        float match_thresh,
        int track_buffer,
        int frame_rate
    );
 
    virtual ~cvedix_bytetrack_node();
 
protected:
    /**
     * @brief Implement ByteTrack association
     *
     * @note Only responsible for assigning track IDs
     */
    virtual void track(int channel_index, const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta,
                        const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        const std::vector<std::vector<float>>& target_embeddings, 
                        std::vector<int>& track_ids) override;
 
private:
    struct ChannelTracker {
        std::unique_ptr<BYTETracker> tracker;
        bool initialized = false;
    };
 
    std::vector<ChannelTracker> m_channel_trackers;
 
    // ByteTrack params
    float m_track_thresh;
    float m_high_thresh;
    float m_match_thresh;
    int   m_track_buffer;
    int   m_fps;
 
private:
    static float iou_tlwh(const cv::Rect2f& a, const cv::Rect2f& b);
};
 
} // namespace cvedix_nodes