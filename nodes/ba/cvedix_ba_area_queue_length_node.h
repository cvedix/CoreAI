/**
 * @file cvedix_ba_area_queue_length_node.h
 * @brief Queue length estimation within polygonal regions
 *
 * Counts the number of tracked objects inside a defined area each frame.
 * Alerts when the count exceeds a threshold.
 *
 * @section queue_algorithm Algorithm
 * 1. Each frame: count targets inside polygon
 * 2. If count > threshold → alert with current queue length
 * 3. Rate-limit notifications by notify_interval
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>

namespace cvedix_nodes {

struct queue_config {
    int queue_threshold;
    std::string name;
    cv::Scalar color;
    cvedix_objects::cvedix_rect_anchor_point anchor_point;

    queue_config()
        : queue_threshold(5), name(""), color(cv::Scalar(255, 165, 0)),
          anchor_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM) {}

    queue_config(int threshold, const std::string& n = "",
                 cv::Scalar c = cv::Scalar(255, 165, 0),
                 cvedix_objects::cvedix_rect_anchor_point anchor =
                     cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM)
        : queue_threshold(threshold), name(n), color(c), anchor_point(anchor) {}
};

class cvedix_ba_area_queue_length_node : public cvedix_node {
private:
    /// @brief Regions per channel
    std::map<int, std::vector<cvedix_objects::cvedix_point>> all_rois;

    /// @brief Configs per channel
    std::map<int, queue_config> all_configs;

    /// @brief Last notification frame per channel
    std::map<int, int> last_notify_frame;

    /// @brief Notification interval in seconds
    int notify_interval_seconds;

    int fps;
    bool need_record_image;
    bool need_record_video;

    mutable std::mutex config_mutex;

    bool is_inside_roi(int channel_id, const cvedix_objects::cvedix_point& pt) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    cvedix_ba_area_queue_length_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
        std::map<int, queue_config> configs,
        int notify_interval_seconds = 10,
        int fps = 30,
        bool need_record_image = true,
        bool need_record_video = false);

    cvedix_ba_area_queue_length_node(
        std::string node_name,
        std::map<int, std::vector<cvedix_objects::cvedix_point>> rois,
        int queue_threshold = 5,
        int notify_interval_seconds = 10,
        int fps = 30,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_area_queue_length_node();
    std::string to_string() override;
};

} // namespace cvedix_nodes
