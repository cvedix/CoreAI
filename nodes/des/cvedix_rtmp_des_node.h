/**
 * @file cvedix_rtmp_des_node.h
 * @brief RTMP streaming destination node
 * 
 * Streams pipeline output to RTMP server (e.g., YouTube Live, Wowza).
 * 
 * @section rtmp_prereq Prerequisites
 * - Compile with `-DCVEDIX_WITH_GSTREAMER`
 * - RTMP server available
 * 
 * @section rtmp_usage Usage
 * @code
 * auto rtmp_des = std::make_shared<cvedix_rtmp_des_node>(
 *     "rtmp_streamer", 0,
 *     "rtmp://192.168.1.50/live/stream1",
 *     {1920, 1080},  // resolution
 *     2048,          // bitrate
 *     true           // OSD enabled
 * );
 * rtmp_des->attach_to({pipeline_node});
 * @endcode
 * 
 * @see cvedix_des_node Base class
 * @see cvedix_rtsp_des_node For RTSP output
 */

#pragma once

#ifdef CVEDIX_WITH_GSTREAMER
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include "cvedix/nodes/common/cvedix_des_node.h"

namespace cvedix_nodes {

    /**
     * @brief RTMP streaming destination node
     * 
     * Pushes H.264 video to RTMP server via GStreamer.
     * 
     * @note Only available with CVEDIX_WITH_GSTREAMER
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_rtmp_des_node: public cvedix_des_node
    {
    private:
        /// @brief GStreamer pipeline template
        std::string gst_template = "appsrc ! videoconvert ! %s bitrate=%d ! h264parse ! flvmux ! rtmpsink location=%s";
        /// @brief OpenCV video writer
        cv::VideoWriter rtmp_writer;

    protected:
        /**
         * @brief Encode and stream frame
         * @param meta Frame to stream
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
         * @param node_name Unique node identifier
         * @param channel_index Channel index
         * @param rtmp_url RTMP URL (e.g., rtmp://server/live/stream)
         * @param resolution_w_h Output resolution
         * @param bitrate Video bitrate (kbps)
         * @param osd Enable OSD overlay
         * @param gst_encoder_name GStreamer encoder (default: x264enc)
         */
        cvedix_rtmp_des_node(std::string node_name, 
                        int channel_index, 
                        std::string rtmp_url, 
                        cvedix_objects::cvedix_size resolution_w_h = {}, 
                        int bitrate = 1024,
                        bool osd = true,
                        std::string gst_encoder_name = "x264enc");

        /// @brief Destructor
        ~cvedix_rtmp_des_node();

        /// @brief Get node description
        virtual std::string to_string() override;
        
        /// @brief RTMP URL
        std::string rtmp_url;
        /// @brief Output resolution
        cvedix_objects::cvedix_size resolution_w_h;
        /// @brief Video bitrate
        int bitrate;
        /// @brief OSD enabled
        bool osd;
        /// @brief GStreamer encoder name
        std::string gst_encoder_name = "x264enc";
    };
}

#endif // CVEDIX_WITH_GSTREAMER