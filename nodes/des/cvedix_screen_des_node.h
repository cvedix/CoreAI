/**
 * @file cvedix_screen_des_node.h
 * @brief Local screen display destination node
 * 
 * Displays pipeline output in a local window with OSD overlay.
 * 
 * @section screen_features Features
 * - Real-time video display
 * - Text and timestamp overlay
 * - Custom display resolution
 * - Auto-selects display sink (GTK, Qt, etc.)
 * 
 * @section screen_usage Usage
 * @code
 * auto screen_des = std::make_shared<cvedix_screen_des_node>(
 *     "display", 0,
 *     true,           // OSD enabled
 *     {1280, 720}     // display size
 * );
 * screen_des->attach_to({pipeline_node});
 * @endcode
 * 
 * @see cvedix_des_node Base class
 */

#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include "cvedix/nodes/common/cvedix_des_node.h"

namespace cvedix_nodes {

    /**
     * @brief Local screen display destination node
     * 
     * Displays video in a local window with OSD overlay.
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_screen_des_node: public cvedix_des_node
    {
    private:
        /// @brief Base GStreamer template with text/time overlay
        const std::string base_gst_template = "appsrc ! videoconvert ! queue ! %s";
        /// @brief Final GStreamer pipeline
        std::string gst_template;
        /// @brief OpenCV video writer
        cv::VideoWriter screen_writer;

        /**
         * @brief Auto-select display sink based on platform
         * @param node_name Name for window title
         * @return GStreamer sink element name
         */
        std::string select_screen_sink(std::string node_name);

    protected:
        /**
         * @brief Display frame in window
         * @param meta Frame to display
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 

        /**
         * @brief Handle control meta
         * @param meta Control meta
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier (also used as window title)
         * @param channel_index Channel index
         * @param osd Enable OSD overlay (default: true)
         * @param display_w_h Display size
         */
        cvedix_screen_des_node(std::string node_name, 
                            int channel_index, 
                            bool osd = true,
                            cvedix_objects::cvedix_size display_w_h = {});

        /// @brief Destructor
        ~cvedix_screen_des_node();

        /// @brief OSD enabled
        bool osd;
        /// @brief Display size
        cvedix_objects::cvedix_size display_w_h;
    };
}