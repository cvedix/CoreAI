/**
 * @file cvedix_motion_detection_node.h
 * @brief Background-subtraction motion detection using OpenCV MOG2
 *
 * Produces a binary foreground mask in cvedix_frame_meta::mask. By default the
 * node preserves the continuous video stream; drop_static_frames can be used
 * when downstream inference should only receive motion-active frames.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <opencv2/video/background_segm.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {

struct motion_detection_config {
    bool enabled = true;
    int history = 200;
    double var_threshold = 16.0;
    double min_area_ratio = 0.0025;
    int warmup_frames = 15;
    int hold_frames = 5;
    int processing_width = 640;
    bool drop_static_frames = false;
};

/**
 * @brief Detect foreground motion regions using a MOG2 background model.
 *
 * The generated mask is a full-frame CV_8UC1 image. Consumers can inspect it
 * for motion metadata, use it for ROI filtering, or enable drop_static_frames
 * to stop static frames before an inference node.
 */
class cvedix_motion_detection_node : public cvedix_node {
public:
    cvedix_motion_detection_node(
        std::string node_name,
        const motion_detection_config &config = motion_detection_config());
    ~cvedix_motion_detection_node() override;

    void update_config(const motion_detection_config &config);
    motion_detection_config get_config() const;

    uint64_t frames_seen() const { return _frames_seen.load(); }
    uint64_t motion_frames() const { return _motion_frames.load(); }
    int last_region_count() const { return _last_region_count.load(); }
    bool motion_active() const { return _motion_active.load(); }

    std::string to_string() override;

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

private:
    void normalize_config_locked();
    void reset_subtractor_locked();

private:
    mutable std::mutex _config_mtx;
    motion_detection_config _config;
    cv::Ptr<cv::BackgroundSubtractorMOG2> _subtractor;
    int _warmup_remaining = 0;
    int _hold_remaining = 0;
    std::atomic<uint64_t> _frames_seen{0};
    std::atomic<uint64_t> _motion_frames{0};
    std::atomic<int> _last_region_count{0};
    std::atomic<bool> _motion_active{false};
};

} // namespace cvedix_nodes
