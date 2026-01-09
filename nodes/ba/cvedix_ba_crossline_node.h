/**
 * @file cvedix_ba_crossline_node.h
 * @brief Crossline detection behavior analysis node
 * 
 * This node detects when tracked objects cross a defined line in the video frame.
 * Commonly used for:
 * - Counting vehicles/pedestrians crossing a boundary
 * - Intrusion detection
 * - Traffic monitoring
 * 
 * @section crossline_overview How It Works
 * 1. Receives tracked targets from upstream tracker node
 * 2. Compares each object's current and previous positions
 * 3. Detects line crossing events
 * 4. Optionally triggers image/video recording
 * 
 * @section crossline_multichannel Multi-Channel Support
 * Each channel can have its own detection line (or no line).
 * Channels without configured lines skip crossline detection.
 * 
 * @section crossline_usage Usage Example
 * @code
 * std::map<int, cvedix_line> lines = {
 *     {0, cvedix_line(cvedix_point(100, 300), cvedix_point(500, 300))}
 * };
 * auto crossline = std::make_shared<cvedix_ba_crossline_node>(
 *     "crossline",
 *     lines,
 *     true,   // record image on crossing
 *     false   // don't record video
 * );
 * crossline->attach_to({tracker_node});
 * @endcode
 * 
 * @see cvedix_ba_jam_node Traffic jam detection
 * @see cvedix_ba_stop_node Stop detection
 */

#pragma once

#include <map>
#include <mutex>
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"

namespace cvedix_nodes {

    /**
     * @brief Crossline detection behavior analysis node
     * 
     * Detects when tracked objects cross user-defined lines in video frames.
     * Supports multiple channels with independent detection lines.
     * 
     * @note Requires tracked objects (must be attached after a tracker node)
     * 
     * @see cvedix_node Base class
     */
    class cvedix_ba_crossline_node: public cvedix_node 
    {
    private:
        /// @brief Crossline counters per channel: channel_id → count
        std::map<int, int> all_total_crossline;

        /// @brief Detection lines per channel (one line per channel max)
        std::map<int, cvedix_objects::cvedix_line> all_lines;

        /// @brief Whether to trigger image recording on crossline event
        bool need_record_image;
        /// @brief Whether to trigger video recording on crossline event
        bool need_record_video;

        /**
         * @brief Check if a point is on one side of a line
         * @param p Point to check
         * @param line Reference line
         * @return true if point is on positive side of line
         */
        bool at_1_side_of_line(cvedix_objects::cvedix_point p, cvedix_objects::cvedix_line line);
        private:
        std::mutex lines_mutex;

    protected:
        /**
         * @brief Process frame meta for crossline detection
         * @param meta Frame meta with tracked targets
         * @return Processed meta (may include record control metas)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Unique node identifier
         * @param lines Detection lines per channel (channel_id → line)
         * @param need_record_image Trigger image recording on crossing (default: true)
         * @param need_record_video Trigger video recording on crossing (default: false)
         */
        cvedix_ba_crossline_node(std::string node_name, 
                            std::map<int, cvedix_objects::cvedix_line> lines,
                            bool need_record_image = true,
                            bool need_record_video = false);

        /// @brief Destructor
        ~cvedix_ba_crossline_node();

        /**
         * @brief Get node description including line configurations
         * @return Human-readable description string
         */
        std::string to_string() override;
         /**
         * @brief Replace all crosslines at runtime
         * @param lines New detection lines per channel
         * @return true if updated successfully
         */
        bool set_lines(const std::map<int, cvedix_objects::cvedix_line>& lines);

        /**
         * @brief Update or insert specific lines at runtime
         * @param lines List of lines to update (by channel_id)
         * @return true if updated successfully
         */
        bool update_lines(const std::vector<std::pair<int, cvedix_objects::cvedix_line>>& lines);

        /**
         * @brief Remove all configured lines
         */
        void clear_lines();

        /**
         * @brief Remove line for a specific channel
         */
        bool remove_line(int channel_id);
    };
}