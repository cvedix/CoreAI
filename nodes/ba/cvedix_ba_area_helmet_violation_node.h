/**
 * @file cvedix_ba_area_helmet_violation_node.h
 * @brief Helmet violation detection for motorcycle riders
 *
 * Detects motorcycle riders not wearing helmets by checking
 * secondary classifier results on detected motorcycle targets.
 *
 * @section hv_algorithm Algorithm
 * 1. Filter targets by motorcycle class ID (primary detector)
 * 2. Check secondary classifier result for "no_helmet" label
 * 3. Require N consecutive frames of "no_helmet" to confirm
 * 4. If confirmed → HELMET violation
 *
 * @section hv_pipeline Required pipeline
 * @code
 * src → detector → tracker → helmet_classifier (secondary) → helmet_violation → broker
 * @endcode
 *
 * @note Requires secondary classifier for helmet/no-helmet detection
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include <map>
#include <mutex>
#include <set>
#include <opencv2/core.hpp>

namespace cvedix_nodes {

/**
 * @brief Configuration for helmet violation detection
 */
struct helmet_config {
    /// @brief Primary class IDs considered as motorcycle (default: {3} for COCO motorcycle)
    std::set<int> motorcycle_class_ids = {3};

    /// @brief Secondary class ID indicating "no helmet"
    int no_helmet_class_id = 0;

    /// @brief Alternative: secondary label string for "no helmet"
    std::string no_helmet_label = "no_helmet";

    /// @brief Minimum consecutive frames of no-helmet to confirm
    int min_confirm_frames = 10;

    /// @brief Optional detection area (empty = whole frame)
    std::vector<cvedix_objects::cvedix_point> detection_area;

    /// @brief Display color
    cv::Scalar color = cv::Scalar(0, 0, 255);

    /// @brief Configuration name
    std::string name = "";

    helmet_config() = default;
};

/**
 * @brief Helmet violation detection node
 */
class cvedix_ba_area_helmet_violation_node : public cvedix_node {
private:
    /// @brief Config per channel
    std::map<int, helmet_config> all_configs;

    /// @brief Consecutive no-helmet frames: channel → track_id → count
    std::map<int, std::map<int, int>> no_helmet_frames;

    /// @brief Already alerted: channel → set of track_ids
    std::map<int, std::set<int>> alerted_tracks;

    bool need_record_image;
    bool need_record_video;

    std::mutex config_mutex;

    bool is_inside_polygon(const cvedix_objects::cvedix_point& p,
                           const std::vector<cvedix_objects::cvedix_point>& polygon) const;

    bool has_no_helmet(const std::shared_ptr<cvedix_objects::cvedix_frame_target>& target,
                       const helmet_config& config) const;

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

public:
    cvedix_ba_area_helmet_violation_node(
        std::string node_name,
        std::map<int, helmet_config> configs,
        bool need_record_image = true,
        bool need_record_video = false);

    ~cvedix_ba_area_helmet_violation_node();
    std::string to_string() override;

    /// @brief Update config at runtime
    bool set_config(int channel_id, const helmet_config& config);
};

} // namespace cvedix_nodes
