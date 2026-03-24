/**
 * @file cvedix_ba_area_dwell_time_node.h
 * @brief Dwell time measurement within polygonal regions
 *
 * Measures how long each tracked object stays inside a defined area
 * and alerts when dwell time exceeds a threshold.
 *
 * @section dwell_algorithm Algorithm
 * 1. When target enters polygon → record entry frame
 * 2. Each frame: dwell_time = (current_frame - entry_frame) / fps
 * 3. Alert when dwell_time > threshold_seconds
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>

namespace cvedix_nodes {

struct dwell_time_config {
    double threshold_seconds;
    std::string name;
    std::string id;
    cv::Scalar color;
    cvedix_objects::cvedix_rect_anchor_point anchor_point;

    dwell_time_config()
        : threshold_seconds(30.0), name(""), id(""), color(cv::Scalar(0, 200, 255)),
          anchor_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM) {}

    dwell_time_config(double sec, const std::string& n = "",
                      cv::Scalar c = cv::Scalar(0, 200, 255),
                      cvedix_objects::cvedix_rect_anchor_point anchor =
                          cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM)
        : threshold_seconds(sec), name(n), color(c), anchor_point(anchor) {}
};

class cvedix_ba_area_dwell_time_node : public cvedix_node {
private:
    struct dwell_state {
        bool inside = false;
        int enter_frame = 0;
        bool alerted = false;
    };

    /// @brief Regions per channel
    std::map<int, std::vector<cvedix_objects::cvedix_point>> all_rois;

    /// @brief Configs per channel
    std::map<int, dwell_time_config> all_configs;

    /// @brief Channel → track_id → state
    std::map<int, std::map<int, dwell_state>> all_states;

    int fps;
    bool need_record_image;
    bool need_record_video;
    bool include_target_crops = false;

    mutable std::mutex config_mutex;

    bool is_inside_roi(int channel_id, const cvedix_objects::cvedix_point& pt) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    cvedix_ba_area_dwell_time_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
        std::map<int, dwell_time_config> configs,
        int fps = 30,
        bool need_record_image = true,
        bool need_record_video = false);

    cvedix_ba_area_dwell_time_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
        double threshold_seconds = 30.0,
        int fps = 30,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_area_dwell_time_node();
    std::string to_string() override;
};

} // namespace cvedix_nodes
