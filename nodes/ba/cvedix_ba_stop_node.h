/**
 * @file cvedix_ba_stop_node.h
 * @brief Individual object stop detection behavior analysis node
 * 
 * This node detects when individual tracked objects stop moving within
 * defined regions. Unlike jam detection, this triggers on each stopped object.
 * 
 * @section stop_algorithm Detection Algorithm
 * 1. Monitor tracked objects within detection region (polygon)
 * 2. Track object movement over consecutive frames
 * 3. If object moves < check_max_distance for check_min_hit_frames → trigger stop event
 * 4. Optionally record image/video evidence per stopped object
 * 
 * @section stop_vs_jam Stop vs Jam Detection
 * | Feature | cvedix_ba_stop_node | cvedix_ba_jam_node |
 * |---------|---------------------|---------------------|
 * | Detects | Individual stopped objects | Traffic jam (multiple stops) |
 * | Trigger | Per-object event | Area-based threshold |
 * | Use case | Illegal parking | Traffic congestion |
 * 
 * @section stop_params Detection Parameters
 * | Parameter | Default | Description |
 * |-----------|---------|-------------|
 * | check_interval_frames | 20 | Frames between checks |
 * | check_min_hit_frames | 50 | Frames object must be stopped |
 * | check_max_distance | 5 | Max pixel movement to be "stopped" |
 * 
 * @section stop_usage Usage Example
 * @code
 * // Define stop detection region (polygon vertices)
 * std::map<int, std::vector<cvedix_point>> regions = {
 *     {0, {{50, 100}, {300, 100}, {300, 350}, {50, 350}}}  // parking zone
 * };
 * 
 * auto stop_node = std::make_shared<cvedix_ba_stop_node>(
 *     "stop_detector",
 *     regions,
 *     true,   // record image on stop
 *     true    // record video on stop
 * );
 * stop_node->attach_to({tracker_node});
 * @endcode
 * 
 * @see cvedix_ba_area_jam_node Traffic jam detection (multiple objects)
 * @see cvedix_ba_line_crossline_node Crossline detection
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
     * @brief Individual object stop detection behavior analysis node
     * 
     * Monitors defined regions and triggers events when individual tracked
     * objects stop moving. Useful for detecting illegal parking or loitering.
     * 
     * @note Requires tracked objects (attach after tracker node)
     * 
     * @see cvedix_node Base class
     * @see cvedix_ba_jam_node For detecting multiple stopped objects
     */
    class cvedix_ba_stop_node: public cvedix_node 
    {
    private:
        /// @brief Detection regions per channel: channel_id → polygon vertices
        std::map<int, std::vector<cvedix_objects::cvedix_point>> all_stop_regions;

        /// @brief Stop tracking status: channel → (track_id → consecutive stopped frames)
        std::map<int, std::map<int, int>> all_stop_checking_status;

        /// @brief Whether to trigger image recording on stop detection
        bool need_record_image;
        /// @brief Whether to trigger video recording on stop detection
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
        const int check_max_distance = 5;

    protected:
        /**
         * @brief Process frame for stop detection
         * @param meta Frame meta with tracked targets
         * @return Processed meta (may include record control metas on stop events)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Unique node identifier
         * @param stop_regions Detection regions per channel (polygon vertices)
         * @param need_record_image Trigger image recording on stop (default: true)
         * @param need_record_video Trigger video recording on stop (default: true)
         */
        cvedix_ba_stop_node(std::string node_name, 
                            std::map<int, std::vector<cvedix_objects::cvedix_point>> stop_regions,
                            bool need_record_image = true,
                            bool need_record_video = true);

        /// @brief Destructor
        ~cvedix_ba_stop_node();

        /**
         * @brief Get node description including region configurations
         * @return Human-readable description string
         */
        std::string to_string() override;
    };
}