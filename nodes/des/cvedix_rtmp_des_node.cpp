
#ifdef CVEDIX_WITH_GSTREAMER
#include <assert.h>
#include <chrono>
#include "cvedix_rtmp_des_node.h"
#include "cvedix/nodes/common/frame_utils.h"
#include "cvedix/utils/cvedix_utils.h"

namespace cvedix_nodes {

    // Build encoder-specific GStreamer pipeline with queue elements for async
    static std::string build_gst_pipeline(const std::string &encoder_name,
                                           int bitrate,
                                           const std::string &rtmp_url) {
        std::string encoder_element;

        if (encoder_name == "nvh264enc") {
            // NVIDIA NVENC H.264 — hardware encoder, very fast
            encoder_element = cvedix_utils::string_format(
                "nvh264enc bitrate=%d preset=low-latency-hq rc-mode=cbr", bitrate);
        } else if (encoder_name == "nvh265enc") {
            // NVIDIA NVENC H.265/HEVC — hardware encoder
            encoder_element = cvedix_utils::string_format(
                "nvh265enc bitrate=%d preset=low-latency-hq rc-mode=cbr", bitrate);
        } else {
            // x264enc (CPU) — use ultrafast preset + zerolatency tune
            encoder_element = cvedix_utils::string_format(
                "x264enc bitrate=%d speed-preset=ultrafast tune=zerolatency", bitrate);
        }

        // h264parse or h265parse based on encoder
        std::string parser = (encoder_name == "nvh265enc") ? "h265parse" : "h264parse";

        // Pipeline with queue elements for async buffering:
        //   appsrc → queue → videoconvert → encoder → parser → flvmux → queue → rtmpsink
        return cvedix_utils::string_format(
            "appsrc ! queue max-size-buffers=3 leaky=downstream ! "
            "videoconvert ! %s ! %s ! flvmux streamable=true ! "
            "queue max-size-buffers=3 leaky=downstream ! rtmpsink location=%s",
            encoder_element.c_str(), parser.c_str(), rtmp_url.c_str());
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

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_rtmp_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));
            
            auto resize_frame = utils::prepare_output_frame(meta, osd, resolution_w_h);

            // Reconnect cooldown: skip frames if we recently failed to connect
            if (!rtmp_writer.isOpened() && reconnect_cooldown_until > std::chrono::steady_clock::now()) {
                return cvedix_des_node::handle_frame_meta(meta);
            }

            // Open writer if not connected
            if (!rtmp_writer.isOpened()) {
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] Connecting to RTMP: %s (attempt #%d)",
                    node_name.c_str(), rtmp_url.c_str(), reconnect_attempts + 1));

                auto ok = rtmp_writer.open(this->gst_pipeline, cv::CAP_GSTREAMER, 0,
                                           meta->fps, {resize_frame.cols, resize_frame.rows});
                if (!ok) {
                    reconnect_attempts++;
                    // Cooldown: wait 3 seconds before retrying
                    reconnect_cooldown_until = std::chrono::steady_clock::now()
                                              + std::chrono::seconds(3);
                    CVEDIX_WARN(cvedix_utils::string_format(
                        "[%s] RTMP connect failed (attempt #%d), retry in 3s",
                        node_name.c_str(), reconnect_attempts));
                    return cvedix_des_node::handle_frame_meta(meta);
                }

                reconnect_attempts = 0;
                write_fail_count = 0;
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[%s] RTMP writer opened: %dx%d @ %dfps -> %s",
                    node_name.c_str(), resize_frame.cols, resize_frame.rows,
                    meta->fps, rtmp_url.c_str()));
            }

            // Write frame — detect failures for reconnect
            try {
                rtmp_writer.write(resize_frame);
                write_fail_count = 0;  // reset on success
            } catch (...) {
                write_fail_count++;
                if (write_fail_count >= 3) {
                    CVEDIX_WARN(cvedix_utils::string_format(
                        "[%s] RTMP write failed %d times, reconnecting...",
                        node_name.c_str(), write_fail_count));
                    rtmp_writer.release();
                    reconnect_cooldown_until = std::chrono::steady_clock::now()
                                              + std::chrono::seconds(3);
                }
            }

            // for general works defined in base class
            return cvedix_des_node::handle_frame_meta(meta);
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_rtmp_des_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
            // for general works defined in base class
            return cvedix_des_node::handle_control_meta(meta);
    }

    std::string cvedix_rtmp_des_node::to_string() {
        // just return rtmp url
        return rtmp_url;
    }
}

#endif // CVEDIX_WITH_GSTREAMER

