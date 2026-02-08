#pragma once
 
#include <map>
#include <unordered_map>
 
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
 
namespace cvedix_nodes {
    /*
     * cvedix_ba_crowding_node
     *
     * Description:
     * - Monitors configured rectangular ROIs per channel and maintains a set
     *   of active tracked objects (by `track_id`) that are currently inside
     *   the ROI.
     * - For each channel the node keeps `enter_ts` and `last_seen_ts` for
     *   each track. If the number of active tracks inside the ROI is >= the
     *   configured `obj_count_threshold` for that channel, the node evaluates
     *   whether all active tracks have been inside the ROI for at least the
     *   configured `alarm_seconds` value. If so, the node sets an alarm and
     *   emits a BA result (and optional image/video record control metas).
     *
     * Behavior notes / how it differs from a "sudden spike" detector:
     * - This implementation detects sustained crowding: every active object
     *   must have been inside the ROI for >= `alarm_seconds` before an alarm
     *   is raised. That means very short-lived spikes will NOT trigger an
     *   alarm unless `alarm_seconds` is set to 0 (or a very small value).
     * - The member `cooldown_seconds` exists but is not currently used in
     *   the implementation.
     *
     * Configuration:
     * - ROIs: `all_rois` (map channel_id -> rect)
     * - Thresholds: `all_obj_count_thresholds` (map channel_id -> int)
     * - Alarm durations: `all_alarm_seconds` (map channel_id -> seconds)
     * - Recording control: `need_record_image`, `need_record_video`
     */
    
    class cvedix_ba_crowding_node : public cvedix_node {
    private:
        struct loiter_state {
            bool inside = false;
            double enter_ts = 0.0;
            double last_seen_ts = 0.0;
        };
        
        struct channel_ctx {
            double now_sec = 0.0;
            std::unordered_map<int, loiter_state> by_track_id;
            bool alarmed = false;
        };
    
        /// ROI polygon per channel (list of points in frame coords)
        std::map<int, std::vector<cvedix_objects::cvedix_point>> all_rois;
    
        /// Object count thresholds per channel
        std::map<int, int> all_obj_count_thresholds;

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
        bool is_inside_roi(int channel_id, const cvedix_objects::cvedix_point& pt) const;
    
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta>
        handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    
    public:
        cvedix_ba_crowding_node(std::string node_name,
                    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
                                std::map<int, int> obj_count_thresholds,
                                std::map<int, double> alarm_seconds,
                                int fps = 30,
                                bool need_record_image = true,
                                bool need_record_video = false);
    
        ~cvedix_ba_crowding_node();
    
        std::string to_string() override;
    };
        
}  // namespace cvedix_nodes