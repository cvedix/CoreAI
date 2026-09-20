/**
 * @file cvedix_rtsp_des_node.h
 * @brief RTSP server destination node (built-in server)
 * 
 * Creates an embedded RTSP server - clients can pull stream directly.
 * No external RTSP server required.
 * 
 * @section rtsp_prereq Prerequisites
 * - Compile with `-DCVEDIX_WITH_GSTREAMER`
 * - Install: `sudo apt-get install libgstrtspserver-1.0-dev gstreamer1.0-rtsp`
 * 
 * @section rtsp_usage Usage
 * @code
 * auto rtsp_des = std::make_shared<cvedix_rtsp_des_node>(
 *     "rtsp_server", 0,
 *     9000,           // RTSP port
 *     "stream1",      // stream name
 *     {1280, 720},    // resolution
 *     512             // bitrate
 * );
 * rtsp_des->attach_to({pipeline_node});
 * // Access: rtsp://localhost:9000/stream1
 * @endcode
 * 
 * @see cvedix_des_node Base class
 * @see cvedix_rtmp_des_node For RTMP push output
 */

#pragma once

#ifdef CVEDIX_WITH_GSTREAMER
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <chrono>
#include "cvedix/nodes/common/cvedix_des_node.h"

namespace cvedix_nodes {

    /**
     * @brief RTSP server destination node
     * 
     * Built-in RTSP server - no external server needed.
     * Clients pull stream via VLC, FFplay, etc.
     * 
     * @note Only available with CVEDIX_WITH_GSTREAMER
     * @note Static server shared between all channel instances
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_rtsp_des_node: public cvedix_des_node {
    private:
        /// @brief GStreamer pipeline template
        /// The internal RTP hop uses IPv4 explicitly: host=localhost resolves to
        /// ::1 first on dual-stack hosts, while udpsrc below binds IPv4, so the
        /// packets would never arrive.
        std::string gst_template = "appsrc ! videoconvert ! %s bitrate=%d ! h264parse ! rtph264pay ! udpsink host=127.0.0.1 port=%d";
        /// @brief OpenCV video writer
        cv::VideoWriter rtsp_writer;

        /**
         * @brief Start RTSP server thread
         */
        void start_rtsp_streaming();
        
        /// @brief Output resolution
        cvedix_objects::cvedix_size resolution_w_h;
        /// @brief Video bitrate
        int bitrate;
        /// @brief OSD enabled
        bool osd;

        /// @brief UDP buffer size
        int udp_buffer_size = 0;
        /// @brief Base UDP port (channel_index added for each channel)
        const int base_udp_port = 7890;

        /// @brief RTSP server port
        int rtsp_port = 9000;
        /// @brief RTSP stream name
        std::string rtsp_name = "";

        /// @brief Shared RTSP server instance
        static GstRTSPServer* rtsp_server;
        /// @brief Main loop serving RTSP requests, run on its own thread
        static GMainLoop* rtsp_main_loop;

        /// @brief GStreamer encoder name
        std::string gst_encoder_name = "x264enc";

        /// @brief Reconnect cooldown timestamp, set after a failed writer open
        std::chrono::steady_clock::time_point reconnect_cooldown_until;
        /// @brief Number of consecutive failed writer open attempts
        int reconnect_attempts = 0;

    protected:
        /**
         * @brief Encode and serve frame
         * @param meta Frame to stream
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param channel_index Channel index
         * @param rtsp_port RTSP server port (default: 9000)
         * @param rtsp_name Stream name in URL
         * @param resolution_w_h Output resolution
         * @param bitrate Video bitrate (kbps)
         * @param osd Enable OSD overlay
         * @param gst_encoder_name GStreamer encoder (default: x264enc)
         */
        cvedix_rtsp_des_node(std::string node_name, 
                        int channel_index, 
                        int rtsp_port = 9000, 
                        std::string rtsp_name = "", 
                        cvedix_objects::cvedix_size resolution_w_h = {}, 
                        int bitrate = 512,
                        bool osd = true,
                        std::string gst_encoder_name = "x264enc");

        /// @brief Destructor
        ~cvedix_rtsp_des_node();

        /// @brief Get node description
        virtual std::string to_string() override;
    };
}

#endif // CVEDIX_WITH_GSTREAMER