/**
 * @file cvedix_ba_intrusion_detection_node.h
 * @brief Intrusion detection behavior analysis node
 *
 * Detects unauthorized access to restricted areas. Emits INTRUSION_START
 * when an object enters a restricted zone and INTRUSION_END when it leaves.
 *
 * Features beyond generic enter/exit:
 * - **Debounce (min_dwell_seconds)**: Only alert if object stays ≥ N seconds
 * - **Cooldown**: Suppress re-alerts for the same track within N seconds
 * - **Event duration**: Automatically computed on INTRUSION_END
 * - **Crop images**: Optional target crop in event output
 *
 * @section intrusion_usage Usage Example
 * @code
 * intrusion_config config;
 * config.id = "84f7990e-238f-4037-ba20-c7fb8c07be36";
 * config.name = "Intrusion Detection Area 1";
 * config.min_dwell_seconds = 0;
 * config.cooldown_seconds = 30;
 *
 * auto node = std::make_shared<cvedix_ba_intrusion_detection_node>(
 *     "intrusion", areas,
 *     std::map<int, std::vector<intrusion_config>>{{0, {config}}},
 *     true, false, true
 * );
 * node->attach_to({tracker_node});
 * @endcode
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <set>
#include <vector>
#include <string>
#include <opencv2/core.hpp>

namespace cvedix_nodes {

/**
 * @brief Configuration for a single intrusion detection area
 */
struct intrusion_config {
    /// @brief UUID identifier for this area (maps to region_id/area_id in events)
    std::string id = "";

    /// @brief Human-readable name for this area
    std::string name = "";

    /// @brief Area color in BGR format (default: red) for visualization
    cv::Scalar color = cv::Scalar(0, 0, 255);

    /// @brief Anchor point on bbox used for point-in-polygon check
    cvedix_objects::cvedix_rect_anchor_point anchor_point =
        cvedix_objects::cvedix_rect_anchor_point::CENTER;

    /// @brief Minimum dwell time (seconds) before triggering alert
    /// 0 = alert immediately on entry
    double min_dwell_seconds = 0;

    /// @brief Cooldown period (seconds) — suppress re-alert for same track
    double cooldown_seconds = 30;
};

/**
 * @brief Intrusion detection behavior analysis node
 *
 * Specialized area-based BA node for restricted zone monitoring.
 * Emits INTRUSION_START / INTRUSION_END events with duration tracking.
 */
class cvedix_ba_intrusion_detection_node : public cvedix_node {
private:
    /// @brief Detection areas: channel → vector of polygons
    std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> all_areas;

    /// @brief Configs per channel per area
    std::map<int, std::vector<intrusion_config>> all_configs;

    /// @brief Enter timestamps: channel → track_id → area_index → epoch_ms
    std::map<int, std::map<int, std::map<int, double>>> enter_timestamps;

    /// @brief Alert timestamps (for cooldown): channel → track_id → area_index → epoch_ms
    std::map<int, std::map<int, std::map<int, double>>> last_alert_timestamps;

    /// @brief Whether INTRUSION_START has been emitted: channel → track_id → set<area_index>
    std::map<int, std::map<int, std::set<int>>> alerted_entries;

    /// @brief Previous area status: channel → track_id → set<area_index>
    std::map<int, std::map<int, std::set<int>>> previous_area_status;

    /// @brief Last seen frame: channel → track_id → frame_index
    std::map<int, std::map<int, int>> last_seen_frame;

    bool need_record_image;
    bool need_record_video;
    bool include_target_crops;

    int inactive_timeout_seconds = 15;

    std::mutex areas_mutex;

    /// @brief Point-in-polygon test (ray-casting)
    bool is_inside_polygon(const cvedix_objects::cvedix_point &p,
                           const std::vector<cvedix_objects::cvedix_point> &polygon);

    /// @brief Get area indices containing the tracked point of a bbox
    std::set<int> get_areas_containing_point(
        const cvedix_objects::cvedix_rect &bbox,
        const std::vector<std::vector<cvedix_objects::cvedix_point>> &areas,
        const std::vector<intrusion_config> &configs);

    /// @brief Get current epoch ms
    static double now_ms();

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor with areas and configs
     */
    cvedix_ba_intrusion_detection_node(
        std::string node_name,
        std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
        std::map<int, std::vector<intrusion_config>> configs,
        bool need_record_image = true,
        bool need_record_video = false,
        bool include_target_crops = false);

    /**
     * @brief Constructor with areas only (default configs)
     */
    cvedix_ba_intrusion_detection_node(
        std::string node_name,
        std::map<int, std::vector<std::vector<cvedix_objects::cvedix_point>>> areas,
        bool need_record_image = true,
        bool need_record_video = false,
        bool include_target_crops = false);

    ~cvedix_ba_intrusion_detection_node();

    std::string to_string() override;
};

} // namespace cvedix_nodes
