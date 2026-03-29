#pragma once
 
#include <map>
#include <mutex>
#include <unordered_map>
 
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
 
namespace cvedix_nodes {
    /*
     * cvedix_ba_area_crowding_node
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
     * - ROIs: `all_rois` (map channel_id -> polygon)
     * - Configs: `all_configs` (map channel_id -> crowding_config)
     * - Recording control: `need_record_image`, `need_record_video`
     */
    
    /**
     * @brief Configuration for crowding detection on a single channel
     */
    struct crowding_config {
        /// @brief Minimum object count to trigger alarm
        int obj_count_threshold = 10;
        
        /// @brief Duration (seconds) objects must remain in ROI before alarm
        double alarm_seconds = 10.0;
        
        /// @brief Optional name/label for this ROI (e.g., "lobby", "entrance")
        std::string name = "";

        /// @brief UUID identifier for this area (used as region_id in events)
        std::string id = "";
        
        /// @brief ROI color in BGR format (default: yellow) for visualization
        cv::Scalar color = cv::Scalar(0, 255, 255);

        /// @brief Anchor point for tracking (default: CENTER)
        cvedix_objects::cvedix_rect_anchor_point anchor_point = cvedix_objects::cvedix_rect_anchor_point::CENTER;
        
        /// @brief Default constructor
        crowding_config() = default;
        
        /// @brief Constructor with threshold and alarm seconds
        crowding_config(int threshold, double seconds)
            : obj_count_threshold(threshold), alarm_seconds(seconds), name(""),
              color(cv::Scalar(0, 255, 255)), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}
        
        /// @brief Constructor with name
        crowding_config(int threshold, double seconds, const std::string &n)
            : obj_count_threshold(threshold), alarm_seconds(seconds), name(n),
              color(cv::Scalar(0, 255, 255)), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}
        
        /// @brief Constructor with name and color
        crowding_config(int threshold, double seconds, const std::string &n,
                       const cv::Scalar &c)
            : obj_count_threshold(threshold), alarm_seconds(seconds), name(n), color(c), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}

        /// @brief Full constructor with all parameters
        crowding_config(int threshold, double seconds, const std::string &n,
                       const cv::Scalar &c, cvedix_objects::cvedix_rect_anchor_point anchor)
            : obj_count_threshold(threshold), alarm_seconds(seconds), name(n), color(c), anchor_point(anchor) {}
    };
    
    class cvedix_ba_area_crowding_node : public cvedix_node {
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
    
        /// Crowding configurations per channel
        std::map<int, crowding_config> all_configs;
    
        /// Runtime states per channel
        std::unordered_map<int, channel_ctx> all_channels;
    
        int fps;
        double cooldown_seconds = 5.0;
        double expire_seconds = 3.0;
    
        bool need_record_image;
        bool need_record_video;

        /// @brief Mutex for thread-safe runtime configuration updates
        mutable std::mutex config_mutex;
    
    public:
        /// @brief Whether to include cropped images of targets in ba_result
        bool include_target_crops;
    
    private:
        bool is_inside_roi(int channel_id, const cvedix_objects::cvedix_point& pt) const;
    
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta>
        handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    
    public:
        cvedix_ba_area_crowding_node(std::string node_name,
                    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
                                std::map<int, crowding_config> configs,
                                int fps = 30,
                                bool need_record_image = true,
                                bool need_record_video = false,
                                bool include_target_crops = false);
        
        cvedix_ba_area_crowding_node(std::string node_name,
                    std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
                                int fps = 30,
                                bool need_record_image = true,
                                bool need_record_video = false,
                                bool include_target_crops = false);
    
        ~cvedix_ba_area_crowding_node();
    
        std::string to_string() override;
        
        /**
         * @brief Replace all ROIs at runtime
         * @param rois New ROI polygons per channel
         * @return true if updated successfully
         */
        bool set_rois(const std::map<int, std::vector<cvedix_objects::cvedix_point>> &rois);
        
        /**
         * @brief Add or update ROI for a specific channel
         * @param channel_id Target channel
         * @param roi Polygon ROI to set
         * @return true if updated successfully
         */
        bool set_channel_roi(int channel_id, const std::vector<cvedix_objects::cvedix_point> &roi);
        
        /**
         * @brief Remove ROI for a specific channel
         * @param channel_id Target channel
         * @return true if channel existed and was removed
         */
        bool remove_channel_roi(int channel_id);
        
        /**
         * @brief Clear all ROIs
         */
        void clear_rois();
        
        /**
         * @brief Get ROI for a specific channel
         * @param channel_id Target channel
         * @return ROI polygon (empty if not found)
         */
        std::vector<cvedix_objects::cvedix_point> get_channel_roi(int channel_id) const;
        
        /**
         * @brief Set crowding configuration for a channel
         * @param channel_id Target channel
         * @param config Crowding configuration
         * @return true if updated successfully
         */
        bool set_config(int channel_id, const crowding_config &config);
        
        /**
         * @brief Get crowding configuration for a channel
         * @param channel_id Target channel
         * @return Configuration (default if not found)
         */
        crowding_config get_config(int channel_id) const;
        
        /**
         * @brief Get number of configured channels
         * @return Number of channels with ROI configuration
         */
        size_t get_channel_count() const;
    };
        
}  // namespace cvedix_nodes