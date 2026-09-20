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
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <chrono>
#include "cvedix/nodes/common/cvedix_des_node.h"

namespace cvedix_nodes {

    /**
     * @brief RTMP streaming destination node
     *
     * Pushes H.264/H.265 video to an RTMP server.
     *
     * @section rtmp_why_owned_pipeline Why this node owns its GStreamer pipeline
     *
     * Every other destination node feeds frames to `cv::VideoWriter` with
     * `cv::CAP_GSTREAMER`. This one does not, and the reason is error reporting.
     * OpenCV's GStreamer writer connects asynchronously: `open()` returns true
     * before the server has been contacted, and a failed push is reported only as
     * a log warning on the GStreamer bus -- `write()` never throws. A node built
     * on it therefore cannot tell a working stream from a dead one, and cannot
     * reconnect. Owning the pipeline lets this node poll the bus for
     * `GST_MESSAGE_ERROR`, which is what makes the reconnect below real rather
     * than decorative.
     *
     * @note Only available with CVEDIX_WITH_GSTREAMER
     *
     * @see cvedix_des_node Base class
     */
    class cvedix_rtmp_des_node: public cvedix_des_node
    {
    private:
        /// @brief GStreamer pipeline (built dynamically per encoder)
        std::string gst_pipeline;

        /**
         * @brief The pipeline this node owns, or nullptr when not streaming.
         *
         * Held as a floating ref from gst_parse_launch; released by
         * teardown_pipeline().
         */
        GstElement* pipeline = nullptr;
        /**
         * @brief The appsrc feeding the pipeline.
         *
         * Holds a ref of its own: gst_bin_get_by_name() returns a new reference
         * rather than borrowing the pipeline's.
         */
        GstElement* appsrc = nullptr;
        /// @brief Bus of @ref pipeline, used to observe errors. Owns a reference.
        GstBus* bus = nullptr;

        /**
         * @brief Build the pipeline and put it in PLAYING.
         * @return true when the pipeline was created and started
         */
        bool start_pipeline();

        /**
         * @brief Drop the pipeline and release every reference it holds.
         *
         * Safe to call when no pipeline exists, and safe to call twice.
         */
        void teardown_pipeline();

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

        /**
         * @brief Stop streaming and release the pipeline.
         *
         * Called by the destructor; also stops the base class threads.
         */
        virtual void deinitialized() override;

    public:
        /**
         * @brief Constructor
         *
         * @param node_name Unique node identifier
         * @param channel_index Channel index
         * @param rtmp_url RTMP URL (e.g., rtmp://server/live/stream)
         * @param resolution_w_h Output resolution
         * @param bitrate Video bitrate (kbps)
         * @param osd Enable OSD overlay
         * @param gst_encoder_name GStreamer encoder (default: x264enc)
         * @param append_channel_suffix
         *        When true (the default), "_<channel_index>" is appended to
         *        @p rtmp_url. The suffix lands at the end of the whole URL, so it
         *        extends the stream key: on channel 0,
         *        `rtmp://host/live/cam` becomes `rtmp://host/live/cam_0`.
         *        Pass false to publish under @p rtmp_url exactly as given --
         *        which is what you want when the stream key is fixed by the
         *        server. The effective URL is always readable via to_string().
         */
        cvedix_rtmp_des_node(std::string node_name,
                        int channel_index,
                        std::string rtmp_url,
                        cvedix_objects::cvedix_size resolution_w_h = {},
                        int bitrate = 1024,
                        bool osd = true,
                        std::string gst_encoder_name = "x264enc",
                        bool append_channel_suffix = true);

        /// @brief Destructor
        ~cvedix_rtmp_des_node();

        /// @brief Get node description
        virtual std::string to_string() override;

        /**
         * @brief Whether a pipeline is running and has not reported an error.
         *
         * Note this reports the absence of errors, not the presence of a
         * receiving server: RTMP has no acknowledgement to wait for.
         */
        bool is_streaming() const;

        /// @brief Number of times the pipeline failed and was torn down
        int failure_count() const;

        /// @brief Number of frames handed to the encoder since construction
        long frames_pushed() const;

        /**
         * @brief The GStreamer pipeline description this node streams with.
         *
         * Exposed because the description is where the streaming behaviour is
         * decided -- where frames may be dropped, how often a keyframe appears
         * and which pixel format the encoder is handed -- and none of that is
         * observable from the outside once the pipeline is running.
         */
        const std::string& pipeline_description() const;

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
        /// @brief Whether to suffix stream key with _<channel_index>
        bool append_channel_suffix = true;

    private:
        /// @brief Reconnect cooldown timestamp
        std::chrono::steady_clock::time_point reconnect_cooldown_until;
        /// @brief Number of reconnect attempts
        int reconnect_attempts = 0;
        /// @brief Consecutive failures, used only to report recovery in the log
        int consecutive_failures = 0;
        /// @brief Frames successfully handed to the encoder
        long frames_pushed_ = 0;
        /// @brief Pipeline failures observed since construction
        int failure_count_ = 0;
        /// @brief Frame rate declared in the appsrc caps, taken from the source
        int stream_fps_ = 25;
    };
}

#endif // CVEDIX_WITH_GSTREAMER
