#pragma once
 
#include <map>
#include <mutex>
#include <unordered_map>
 
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
 
namespace cvedix_nodes {
 
    /**
     * @brief Configuration for loitering detection on a single channel
     */
    struct loitering_config {
        /// @brief Duration (seconds) object must remain in ROI before alarm
        double alarm_seconds = 10.0;
        
        /// @brief Optional name/label for this ROI (e.g., "parking lot", "entrance")
        std::string name = "";
        
        /// @brief ROI color in BGR format (default: orange) for visualization
        cv::Scalar color = cv::Scalar(0, 165, 255);

        /// @brief Anchor point for tracking (default: CENTER)
        cvedix_objects::cvedix_rect_anchor_point anchor_point = cvedix_objects::cvedix_rect_anchor_point::CENTER;
        
        /// @brief Default constructor
        loitering_config() = default;
        
        /// @brief Constructor with alarm seconds
        loitering_config(double seconds)
            : alarm_seconds(seconds), name(""),
              color(cv::Scalar(0, 165, 255)), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}
        
        /// @brief Constructor with name
        loitering_config(double seconds, const std::string &n)
            : alarm_seconds(seconds), name(n),
              color(cv::Scalar(0, 165, 255)), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}
        
        /// @brief Constructor with name and color
        loitering_config(double seconds, const std::string &n,
                        const cv::Scalar &c)
            : alarm_seconds(seconds), name(n), color(c), anchor_point(cvedix_objects::cvedix_rect_anchor_point::CENTER) {}
        
        /// @brief Full constructor with all parameters
        loitering_config(double seconds, const std::string &n,
                        const cv::Scalar &c, cvedix_objects::cvedix_rect_anchor_point anchor)
            : alarm_seconds(seconds), name(n), color(c), anchor_point(anchor) {}
    };
 
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
 
    /// ROI polygon per channel (list of points in frame coords)
    std::map<int, std::vector<cvedix_objects::cvedix_point>> all_rois;
 
    /// Loitering configurations per channel
    std::map<int, loitering_config> all_configs;
 
    /// Runtime states per channel
    std::unordered_map<int, channel_ctx> all_channels;
 
    int fps;
    double cooldown_seconds = 5.0;
    double expire_seconds = 3.0;
 
    bool need_record_image;
    bool need_record_video;
    
    /// @brief Mutex for thread-safe runtime configuration updates
    mutable std::mutex config_mutex;
 
private:
    bool is_inside_roi(int channel_id, const cvedix_objects::cvedix_point& pt) const;
 
protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta>
    handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
 
public:
    cvedix_ba_loitering_node(std::string node_name,
                            std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
                            std::map<int, loitering_config> configs,
                            int fps = 30,
                            bool need_record_image = true,
                            bool need_record_video = false);
    
    cvedix_ba_loitering_node(std::string node_name,
                            std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
                            int fps = 30,
                            bool need_record_image = true,
                            bool need_record_video = false);
 
    ~cvedix_ba_loitering_node();
 
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
     * @brief Set loitering configuration for a channel
     * @param channel_id Target channel
     * @param config Loitering configuration
     * @return true if updated successfully
     */
    bool set_config(int channel_id, const loitering_config &config);
    
    /**
     * @brief Get loitering configuration for a channel
     * @param channel_id Target channel
     * @return Configuration (default if not found)
     */
    loitering_config get_config(int channel_id) const;
    
    /**
     * @brief Get number of configured channels
     * @return Number of channels with ROI configuration
     */
    size_t get_channel_count() const;
};
 
} // namespace cvedix_nodes