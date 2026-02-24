/**
 * @file cvedix_ba_area_parking_violation_node.h
 * @brief Parking violation detection in no-parking zones
 *
 * Detects vehicles that stop/park within defined no-parking areas
 * for longer than an allowed duration.
 *
 * @section parking_algorithm Algorithm
 * 1. Track objects inside no-parking polygon
 * 2. If object moves < max_movement for consecutive frames → "parked"
 * 3. If parked duration > allowed_seconds → PARKING VIOLATION
 * 4. Supports per-zone configuration (time limits, vehicle class filtering)
 *
 * @section parking_vs_stop Parking vs Stop Detection
 * | Feature | parking_violation | ba_stop_node |
 * |---------|-------------------|--------------|
 * | Zone-aware | Yes (no-parking polygon) | Yes (region) |
 * | Time-based | Yes (configurable per zone) | Fixed frame count |
 * | Class filtering | Yes (vehicle classes only) | No |
 * | Use case | No-parking enforcement | Generic stop detection |
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <set>

namespace cvedix_nodes {

struct parking_zone_config {
    /// @brief Max seconds allowed before violation
    double allowed_seconds;

    /// @brief Zone name for labeling
    std::string name;

    /// @brief Zone color for OSD
    cv::Scalar color;

    /// @brief Max pixel movement to be considered "stopped"
    float max_movement;

    /// @brief Class IDs to monitor (empty = all classes)
    std::set<int> vehicle_class_ids;

    /// @brief Anchor point for position check
    cvedix_objects::cvedix_rect_anchor_point anchor_point;

    parking_zone_config()
        : allowed_seconds(30.0), name(""), color(cv::Scalar(0, 0, 255)),
          max_movement(8.0f),
          anchor_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM) {}

    parking_zone_config(double seconds, const std::string& n = "",
                        cv::Scalar c = cv::Scalar(0, 0, 255),
                        float max_mov = 8.0f,
                        std::set<int> classes = {})
        : allowed_seconds(seconds), name(n), color(c),
          max_movement(max_mov), vehicle_class_ids(classes),
          anchor_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM) {}
};

class cvedix_ba_area_parking_violation_node : public cvedix_node {
private:
    struct park_state {
        bool inside = false;
        int enter_frame = 0;
        int last_check_frame = 0;
        float last_x = 0, last_y = 0;
        int stopped_frames = 0;
        bool alerted = false;
    };

    /// @brief No-parking zones per channel
    std::map<int, std::vector<cvedix_objects::cvedix_point>> all_zones;

    /// @brief Config per channel
    std::map<int, parking_zone_config> all_configs;

    /// @brief Channel → track_id → state
    std::map<int, std::map<int, park_state>> all_states;

    int fps;
    bool need_record_image;
    bool need_record_video;

    /// @brief Check interval (frames)
    int check_interval;

    mutable std::mutex config_mutex;

    bool point_in_poly(const cvedix_objects::cvedix_point& p,
                       const std::vector<cvedix_objects::cvedix_point>& poly) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    /**
     * @brief Constructor with per-channel config
     */
    cvedix_ba_area_parking_violation_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> zones,
        std::map<int, parking_zone_config> configs,
        int fps = 30,
        bool need_record_image = true,
        bool need_record_video = true);

    /**
     * @brief Simple constructor (same config for all channels)
     */
    cvedix_ba_area_parking_violation_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> zones,
        double allowed_seconds = 30.0,
        int fps = 30,
        bool need_record_image = true,
        bool need_record_video = true);

    ~cvedix_ba_area_parking_violation_node();
    std::string to_string() override;

    /// @brief Runtime zone update
    bool set_zone(int channel_id,
                  const std::vector<cvedix_objects::cvedix_point>& zone,
                  const parking_zone_config& config);

    bool remove_zone(int channel_id);
    void clear_zones();
};

} // namespace cvedix_nodes
