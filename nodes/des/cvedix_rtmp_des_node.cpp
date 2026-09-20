

#ifdef CVEDIX_WITH_GSTREAMER
#include <chrono>
#include <csignal>
#include <mutex>
#include <string>
#include "cvedix_rtmp_des_node.h"
#include "cvedix/nodes/common/frame_utils.h"
#include "cvedix/utils/cvedix_utils.h"

namespace cvedix_nodes {

    namespace {
        /// @brief Name given to the appsrc element so it can be looked up again
        constexpr const char* kAppSrcName = "cvedix_appsrc";

        /// @brief How long to wait after a failure before building a new pipeline
        constexpr int kReconnectCooldownSeconds = 3;

        /**
         * @brief Initialise GStreamer exactly once, whichever thread gets here first.
         *
         * gst_init() is not thread-safe and nodes are built from arbitrary
         * threads, so the call is funnelled through std::call_once.
         *
         * SIGPIPE is ignored for the whole process here, and that is not
         * optional. rtmpsink writes to its socket with send() and does not pass
         * MSG_NOSIGNAL, so the moment the RTMP server closes the connection the
         * next write raises SIGPIPE and the default action kills the process --
         * before this node can poll its bus and reconnect, which is precisely
         * the case the reconnect exists for. Measured: stopping the RTMP server
         * mid-stream ended the process with status 141 (128+SIGPIPE).
         *
         * The disposition cannot be confined to this node: the write happens on
         * a GStreamer-owned streaming thread, and signals are per-process. The
         * cost is that a broken pipe elsewhere in the host process now surfaces
         * as EPIPE from write() instead of as a fatal signal.
         */
        void ensure_gst_initialized() {
            static std::once_flag once;
            std::call_once(once, [] {
                signal(SIGPIPE, SIG_IGN);
                gst_init(nullptr, nullptr);
            });
        }
    }

    // Build encoder-specific GStreamer pipeline with queue elements for async
    static std::string build_gst_pipeline(const std::string &encoder_name,
                                           int bitrate,
                                           const std::string &rtmp_url) {
        std::string encoder_element;

        // Every encoder is pinned to a short GOP. A viewer that loses one frame
        // keeps showing artefacts until the next keyframe, so the interval sets
        // how long a glitch survives on screen. The defaults are far too long
        // for that: x264enc leaves key-int-max at 0, which x264 reads as "decide
        // yourself" and resolves to 250 frames -- 17s at 15fps, and NVENC's own
        // default is 250 as well. At 30 frames the damage is bounded to ~2s.
        constexpr int kKeyframeInterval = 30;
        if (encoder_name == "nvh264enc") {
            // NVIDIA NVENC H.264 — hardware encoder, very fast
            encoder_element = cvedix_utils::string_format(
                "nvh264enc bitrate=%d preset=low-latency-hq rc-mode=cbr gop-size=%d",
                bitrate, kKeyframeInterval);
        } else if (encoder_name == "nvh265enc") {
            // NVIDIA NVENC H.265/HEVC — hardware encoder
            encoder_element = cvedix_utils::string_format(
                "nvh265enc bitrate=%d preset=low-latency-hq rc-mode=cbr gop-size=%d",
                bitrate, kKeyframeInterval);
        } else {
            // x264enc (CPU) — use ultrafast preset + zerolatency tune
            encoder_element = cvedix_utils::string_format(
                "x264enc bitrate=%d speed-preset=ultrafast tune=zerolatency key-int-max=%d",
                bitrate, kKeyframeInterval);
        }

        // h264parse or h265parse based on encoder
        std::string parser = (encoder_name == "nvh265enc") ? "h265parse" : "h264parse";

        // Pipeline with queue elements for async buffering:
        //   appsrc → queue → videoconvert → I420 → encoder → parser → flvmux → queue → rtmpsink
        // The appsrc carries a name so start_pipeline() can find it again; the
        // rest is unchanged from when OpenCV built this same string.
        //
        // Only the queue before the encoder may drop. What it holds are raw
        // frames, so losing one costs a frame of video and nothing else. The
        // queue after flvmux holds muxed FLV tags, and dropping one of those
        // tears the bitstream: the decoder loses the reference for every frame
        // that follows and paints artefacts until the next keyframe. It was
        // leaky=downstream too, and that is what made the published stream look
        // broken whenever the encoder fell behind. It keeps the backpressure
        // instead, which the leaky queue upstream absorbs safely.
        //
        // The format is pinned to I420 rather than left to videoconvert. Given
        // a free choice videoconvert and x264enc agree on Y444, which encodes as
        // High 4:4:4 Predictive -- a profile many players, browsers and
        // downstream repackagers refuse to decode. 4:2:0 is what every RTMP
        // consumer expects, and it also cuts the encoder's work.
        return cvedix_utils::string_format(
            "appsrc name=%s ! queue max-size-buffers=3 leaky=downstream ! "
            "videoconvert ! video/x-raw,format=I420 ! %s ! %s ! flvmux streamable=true ! "
            "queue max-size-buffers=3 ! rtmpsink location=%s",
            kAppSrcName, encoder_element.c_str(), parser.c_str(), rtmp_url.c_str());
    }

    cvedix_rtmp_des_node::cvedix_rtmp_des_node(std::string node_name,
                                        int channel_index,
                                        std::string rtmp_url,
                                        cvedix_objects::cvedix_size resolution_w_h,
                                        int bitrate,
                                        bool osd,
                                        std::string gst_encoder_name,
                                        bool append_channel_suffix):
                                        cvedix_des_node(node_name, channel_index),
                                        rtmp_url(rtmp_url),
                                        resolution_w_h(resolution_w_h),
                                        bitrate(bitrate),
                                        osd(osd),
                                        gst_encoder_name(gst_encoder_name),
                                        append_channel_suffix(append_channel_suffix) {
        ensure_gst_initialized();

        if (this->append_channel_suffix) {
            this->rtmp_url = this->rtmp_url + "_" + std::to_string(channel_index);
        }
        this->gst_pipeline = build_gst_pipeline(gst_encoder_name, bitrate, this->rtmp_url);
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), gst_pipeline.c_str()));
        this->initialized();
    }

    cvedix_rtmp_des_node::~cvedix_rtmp_des_node() {
        deinitialized();
    }

    void cvedix_rtmp_des_node::deinitialized() {
        teardown_pipeline();
        cvedix_node::deinitialized();
    }

    bool cvedix_rtmp_des_node::start_pipeline() {
        GError* error = nullptr;
        pipeline = gst_parse_launch(this->gst_pipeline.c_str(), &error);

        if (pipeline == nullptr) {
            CVEDIX_WARN(cvedix_utils::string_format(
                "[%s] cannot build RTMP pipeline: %s",
                node_name.c_str(), error != nullptr ? error->message : "unknown error"));
            if (error != nullptr) g_error_free(error);
            return false;
        }
        if (error != nullptr) {
            // A recoverable parse warning; the pipeline is still usable.
            CVEDIX_WARN(cvedix_utils::string_format(
                "[%s] RTMP pipeline warning: %s", node_name.c_str(), error->message));
            g_error_free(error);
        }

        appsrc = gst_bin_get_by_name(GST_BIN(pipeline), kAppSrcName);
        if (appsrc == nullptr) {
            CVEDIX_WARN(cvedix_utils::string_format(
                "[%s] RTMP pipeline has no '%s' element", node_name.c_str(), kAppSrcName));
            teardown_pipeline();
            return false;
        }

        bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

        // The caps tell videoconvert what it is being handed. Without them the
        // pipeline cannot negotiate and every push is refused.
        //
        // The frame rate has to be the source's, not a guess: it is the timebase
        // the buffers below are stamped in, so a caps value that disagrees with
        // the PTS makes flvmux misjudge the stream's speed, and a downstream
        // player either stalls or races. 25 was hardcoded here while the pushes
        // used meta->fps, and the source runs at 15.
        const int width = resolution_w_h.width;
        const int height = resolution_w_h.height;
        const int fps = stream_fps_;
        GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                            "format", G_TYPE_STRING, "BGR",
                                            "width", G_TYPE_INT, width,
                                            "height", G_TYPE_INT, height,
                                            "framerate", GST_TYPE_FRACTION, fps, 1,
                                            nullptr);
        gst_app_src_set_caps(GST_APP_SRC(appsrc), caps);
        gst_caps_unref(caps);

        // Timestamps are set by this node from its own frame counter, so appsrc
        // must not stamp buffers itself.
        g_object_set(appsrc,
                     "is-live", TRUE,
                     "format", GST_FORMAT_TIME,
                     "do-timestamp", FALSE,
                     nullptr);

        gst_element_set_state(pipeline, GST_STATE_PLAYING);

        if (consecutive_failures > 0) {
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] RTMP restarted after %d failure(s) -> %s",
                node_name.c_str(), consecutive_failures, to_string().c_str()));
            consecutive_failures = 0;
        } else {
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] RTMP pipeline started -> %s", node_name.c_str(), to_string().c_str()));
        }
        return true;
    }

    void cvedix_rtmp_des_node::teardown_pipeline() {
        // Each pointer is nulled straight after its unref so a second call, or a
        // call from the destructor, cannot double-free.
        if (pipeline != nullptr) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
        }
        if (appsrc != nullptr) {
            gst_object_unref(appsrc);
            appsrc = nullptr;
        }
        if (bus != nullptr) {
            gst_object_unref(bus);
            bus = nullptr;
        }
        if (pipeline != nullptr) {
            gst_object_unref(pipeline);
            pipeline = nullptr;
        }
    }

    /**
     * @brief Drain the bus looking for an error or end-of-stream.
     *
     * This is the whole point of owning the pipeline. OpenCV's VideoWriter
     * receives these same messages but only prints them, so a node built on it
     * cannot tell a live stream from a dead one. gst_bus_pop_filtered() does not
     * block, so draining per frame costs nothing and needs no extra thread.
     *
     * @param reason Out-parameter; the GStreamer error text when one was found
     * @return true when the pipeline has failed and must be rebuilt
     */
    static bool drain_bus_for_errors(GstBus* bus, std::string& reason) {
        bool failed = false;
        GstMessage* message = nullptr;
        const GstMessageType wanted =
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

        while ((message = gst_bus_pop_filtered(bus, wanted)) != nullptr) {
            if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR && !failed) {
                GError* error = nullptr;
                gchar* debug = nullptr;
                gst_message_parse_error(message, &error, &debug);
                reason = error != nullptr ? error->message : "unknown GStreamer error";
                if (error != nullptr) g_error_free(error);
                g_free(debug);
                failed = true;
            } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS && !failed) {
                // The sink closed the stream -- for rtmpsink this means the
                // server went away.
                reason = "stream ended (EOS)";
                failed = true;
            }
            gst_message_unref(message);
        }
        return failed;
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta>
        cvedix_rtmp_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));

            // Taken before the pipeline starts below: start_pipeline() bakes the
            // frame rate into the appsrc caps, and on the first frame that runs
            // from inside this same call. Latched once so a source that reports
            // nothing on some frame cannot silently move the stream back to the
            // fallback rate.
            if (meta->fps > 0) {
                stream_fps_ = meta->fps;
            }

            auto resize_frame = utils::prepare_output_frame(meta, osd, resolution_w_h);

            // Skip frames while backing off from a failure, so a dead server
            // does not cost a pipeline rebuild per frame.
            if (pipeline == nullptr && reconnect_cooldown_until > std::chrono::steady_clock::now()) {
                return cvedix_des_node::handle_frame_meta(meta);
            }

            if (pipeline == nullptr) {
                reconnect_attempts++;
                if (!start_pipeline()) {
                    consecutive_failures++;
                    failure_count_++;
                    reconnect_cooldown_until = std::chrono::steady_clock::now()
                                              + std::chrono::seconds(kReconnectCooldownSeconds);
                    return cvedix_des_node::handle_frame_meta(meta);
                }
            } else {
                // Check the bus before pushing: a pipeline that failed on an
                // earlier frame should be rebuilt, not written to.
                std::string reason;
                if (drain_bus_for_errors(bus, reason)) {
                    consecutive_failures++;
                    failure_count_++;
                    CVEDIX_WARN(cvedix_utils::string_format(
                        "[%s] RTMP error: %s (failure #%d), retry in %ds",
                        node_name.c_str(), reason.c_str(), failure_count_,
                        kReconnectCooldownSeconds));
                    teardown_pipeline();
                    reconnect_cooldown_until = std::chrono::steady_clock::now()
                                              + std::chrono::seconds(kReconnectCooldownSeconds);
                    return cvedix_des_node::handle_frame_meta(meta);
                }
            }

            // Same source as the caps above, so the buffers are stamped in the
            // timebase the pipeline negotiated.
            const int fps = stream_fps_;

            // gst_buffer_fill() copies from a flat pointer, so a view into a
            // larger Mat (a ROI, say) has to be made contiguous first.
            if (!resize_frame.isContinuous()) {
                resize_frame = resize_frame.clone();
            }
            if (resize_frame.empty()) {
                return cvedix_des_node::handle_frame_meta(meta);
            }

            const size_t size = static_cast<size_t>(resize_frame.total()) * resize_frame.elemSize();
            GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
            if (buffer == nullptr) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] cannot allocate %zu-byte GstBuffer", node_name.c_str(), size));
                return cvedix_des_node::handle_frame_meta(meta);
            }
            gst_buffer_fill(buffer, 0, resize_frame.data, size);

            // Timestamps come from a counter that only ever moves forward, not
            // from meta->frame_index: a source that restarts (or a reconnect)
            // would otherwise replay indices and hand flvmux a PTS going
            // backwards.
            GST_BUFFER_PTS(buffer) = gst_util_uint64_scale(frames_pushed_, GST_SECOND, fps);
            GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, fps);

            // push_buffer takes ownership of the buffer whether or not it
            // succeeds, so it is never unref'd here.
            const GstFlowReturn flow = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
            if (flow != GST_FLOW_OK) {
                consecutive_failures++;
                failure_count_++;
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] RTMP push refused (%s), retry in %ds",
                    node_name.c_str(), gst_flow_get_name(flow), kReconnectCooldownSeconds));
                teardown_pipeline();
                reconnect_cooldown_until = std::chrono::steady_clock::now()
                                          + std::chrono::seconds(kReconnectCooldownSeconds);
                return cvedix_des_node::handle_frame_meta(meta);
            }

            frames_pushed_++;
            reconnect_attempts = 0;

            // for general works defined in base class
            return cvedix_des_node::handle_frame_meta(meta);
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta>
        cvedix_rtmp_des_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
            // for general works defined in base class
            return cvedix_des_node::handle_control_meta(meta);
    }

    bool cvedix_rtmp_des_node::is_streaming() const {
        // A failed pipeline is torn down immediately, so a live pointer is the
        // same statement as "no error has been reported".
        return pipeline != nullptr;
    }

    int cvedix_rtmp_des_node::failure_count() const {
        return failure_count_;
    }

    long cvedix_rtmp_des_node::frames_pushed() const {
        return frames_pushed_;
    }

    const std::string& cvedix_rtmp_des_node::pipeline_description() const {
        return gst_pipeline;
    }

    std::string cvedix_rtmp_des_node::to_string() {
        // just return rtmp url
        return rtmp_url;
    }
}

#endif // CVEDIX_WITH_GSTREAMER

