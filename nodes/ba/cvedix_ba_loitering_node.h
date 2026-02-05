#pragma once
 
#include <map>
#include <unordered_map>
 
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_rect.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
 
namespace cvedix_nodes {
 
class cvedix_ba_loitering_node : public cvedix_node {
private:
    struct loiter_state {
        bool inside = false;
        double enter_ts = 0.0;
        double last_seen_ts = 0.0;
        bool alarmed = false;
    };
 
    struct channel_ctx {
        double now_sec = 0.0;
        std::unordered_map<int, loiter_state> by_track_id;
    };
 
    /// ROI per channel
    std::map<int, cvedix_objects::cvedix_rect> all_rois;
 
    /// Alarm seconds per channel
    std::map<int, double> all_alarm_seconds;
 
    /// Runtime states per channel
    std::unordered_map<int, channel_ctx> all_channels;
 
    int fps;
    double cooldown_seconds = 5.0;
    double expire_seconds = 3.0;
 
    bool need_record_image;
    bool need_record_video;
 
private:
    bool is_inside_roi(int channel_id, const cvedix_objects::cvedix_rect& r) const;
 
protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta>
    handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
 
public:
    cvedix_ba_loitering_node(std::string node_name,
                            std::map<int, cvedix_objects::cvedix_rect> rois,
                            std::map<int, double> alarm_seconds,
                            int fps = 30,
                            bool need_record_image = true,
                            bool need_record_video = false);
 
    ~cvedix_ba_loitering_node();
 
    std::string to_string() override;
};
 
} // namespace cvedix_nodes