/**
 * @file cvedix_ba_area_no_entry_zone_node.h
 * @brief No-entry zone detection with vehicle class and time schedule filtering
 *
 * Detects restricted vehicles entering no-entry zones during restricted hours.
 * Example: trucks banned from downtown during peak hours.
 *
 * @section ne_algorithm Algorithm
 * 1. Define zone polygon with per-class time schedules
 * 2. If vehicle of restricted class enters zone during restricted time → violation
 * 3. Supports day-of-week and hour/minute schedules
 *
 * @section ne_usage Usage
 * @code
 * time_schedule peak = {7, 0, 9, 0, {1,2,3,4,5}}; // Mon-Fri 7:00-9:00
 * no_entry_config cfg;
 * cfg.zone_polygon = {{0,0},{640,0},{640,480},{0,480}};
 * cfg.class_schedules[2] = {peak}; // class 2 = truck
 * auto node = std::make_shared<cvedix_ba_area_no_entry_zone_node>(
 *     "no_entry", {{0, cfg}});
 * @endcode
 *
 * @see cvedix_ba_area_enter_exit_node Area entry detection
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <set>
#include <chrono>
#include <ctime>
#include <opencv2/core.hpp>

namespace cvedix_nodes {

/**
 * @brief Time schedule for restriction periods
 */
struct time_schedule {
    int start_hour = 0;    ///< Start hour (0-23)
    int start_minute = 0;  ///< Start minute (0-59)
    int end_hour = 23;     ///< End hour (0-23)
    int end_minute = 59;   ///< End minute (0-59)
    /// @brief Days of week (0=Sunday, 1=Monday ... 6=Saturday). Empty = every day.
    std::set<int> days_of_week;

    time_schedule() = default;

    time_schedule(int sh, int sm, int eh, int em,
                  const std::set<int>& days = {})
        : start_hour(sh), start_minute(sm),
          end_hour(eh), end_minute(em), days_of_week(days) {}
};

/**
 * @brief Configuration for a no-entry zone
 */
struct no_entry_config {
    /// @brief Zone polygon
    std::vector<cvedix_objects::cvedix_point> zone_polygon;

    /// @brief Restriction schedules per class: class_id → list of schedules
    /// If a class_id is present → restricted during those schedules
    /// If empty → always restricted for ALL classes
    std::map<int, std::vector<time_schedule>> class_schedules;

    /// @brief Zone name
    std::string zone_name = "";

    /// @brief Display color
    cv::Scalar color = cv::Scalar(0, 0, 255);

    /// @brief Anchor point for position check
    cvedix_objects::cvedix_rect_anchor_point anchor_point =
        cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM;

    no_entry_config() = default;
};

/**
 * @brief No-entry zone detection node
 */
class cvedix_ba_area_no_entry_zone_node : public cvedix_node {
private:
    /// @brief Configs per channel
    std::map<int, no_entry_config> all_configs;

    /// @brief Already alerted: channel → set of track_ids
    std::map<int, std::set<int>> alerted_tracks;

    bool need_record_image;
    bool need_record_video;

    std::mutex config_mutex;

    bool is_inside_polygon(const cvedix_objects::cvedix_point& p,
                           const std::vector<cvedix_objects::cvedix_point>& polygon) const;

    bool is_restricted_now(int class_id,
                           const std::map<int, std::vector<time_schedule>>& schedules) const;

    bool is_within_schedule(const time_schedule& sched, int hour, int minute, int wday) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    cvedix_ba_area_no_entry_zone_node(
        std::string node_name,
        std::map<int, no_entry_config> configs,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_area_no_entry_zone_node();
    std::string to_string() override;

    /// @brief Update config at runtime
    bool set_config(int channel_id, const no_entry_config& config);
};

} // namespace cvedix_nodes
