/**
 * @file cvedix_ba_jam_node.h
 * @brief Traffic jam detection behavior analysis node
 * 
 * This node detects traffic jam conditions by monitoring stopped/slow-moving
 * objects within defined regions over time.
 * 
 * @section jam_algorithm Detection Algorithm
 * 1. Monitor tracked objects within detection region
 * 2. Count objects that remain stationary (< check_max_distance movement)
 * 3. If enough objects stay stopped for minimum time → trigger jam alert
 * 4. Optionally record image/video evidence
 * 
 * @section jam_params Detection Parameters
 * | Parameter | Default | Description |
 * |-----------|---------|-------------|
 * | check_interval_frames | 20 | Frames between checks |
 * | check_min_hit_frames | 50 | Frames object must be stopped |
 * | check_max_distance | 8 | Max pixel movement to be "stopped" |
 * | check_min_stops | 8 | Min stopped objects for jam |
 * | check_notify_interval | 10 | Seconds between notifications |
 * 
 * @section jam_usage Usage Example
 * @code
 * // Define jam detection region (polygon vertices)
 * std::map<int, std::vector<cvedix_point>> regions = {
 *     {0, {{100, 200}, {400, 200}, {400, 400}, {100, 400}}}
 * };
 * 
 * auto jam_node = std::make_shared<cvedix_ba_jam_node>(
 *     "jam_detector",
 *     regions,
 *     true,   // record image on jam
 *     true    // record video on jam
 * );
 * jam_node->attach_to({tracker_node});
 * @endcode
 * 
 * @see cvedix_ba_crossline_node Crossline detection
 * @see cvedix_ba_stop_node Individual stop detection
 */

#pragma once

#include <map>
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"

namespace cvedix_nodes {

    /**
     * @brief Traffic jam detection behavior analysis node
     * 
     * Monitors defined regions for traffic jam conditions by tracking
     * stopped/slow-moving objects over time. Supports multi-channel operation.
     * 
     * @note Requires tracked objects (attach after tracker node)
     * 
     * @see cvedix_node Base class
     */
    class cvedix_ba_jam_node: public cvedix_node 
    {
    private:
        /// @brief Detection regions per channel: channel_id → polygon vertices
        std::map<int, std::vector<cvedix_objects::cvedix_point>> all_jam_regions;

        /// @brief Stop tracking status: channel → (track_id → consecutive stopped frames)
        std::map<int, std::map<int, int>> all_stop_checking_status;

        /// @brief Current jam status per channel
        std::map<int, bool> all_jam_results;

        /// @brief Last notification frame index per channel (for rate limiting)
        std::map<int, int> all_last_notifys;

        /// @brief Whether to trigger image recording on jam detection
        bool need_record_image;
        /// @brief Whether to trigger video recording on jam detection
        bool need_record_video;

        /**
         * @brief Check if point is inside polygon region
         * @param p Point to check
         * @param region Polygon vertices
         * @return true if point is inside region
         */
        bool point_in_poly(cvedix_objects::cvedix_point p, std::vector<cvedix_objects::cvedix_point> region);

        /// @brief Frames between position checks
        const int check_interval_frames = 20;
        /// @brief Minimum frames object must be stopped (25 FPS * 2 sec)
        const int check_min_hit_frames = 25 * 2;
        /// @brief Maximum pixel movement to be considered "stopped"
        const int check_max_distance = 8;
        /// @brief Minimum stopped objects to trigger jam alert
        const int check_min_stops = 8;
        /// @brief Minimum seconds between jam notifications
        const int check_notify_interval = 10;

    protected:
        /**
         * @brief Process frame for jam detection
         * @param meta Frame meta with tracked targets
         * @return Processed meta (may include record control metas)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Unique node identifier
         * @param jam_regions Detection regions per channel (polygon vertices)
         * @param need_record_image Trigger image recording on jam (default: true)
         * @param need_record_video Trigger video recording on jam (default: true)
         */
        cvedix_ba_jam_node(std::string node_name, 
                            std::map<int, std::vector<cvedix_objects::cvedix_point>> jam_regions,
                            bool need_record_image = true,
                            bool need_record_video = true);

        /// @brief Destructor
        ~cvedix_ba_jam_node();

        /**
         * @brief Get node description including region configurations
         * @return Human-readable description string
         */
        std::string to_string() override;
    };
}