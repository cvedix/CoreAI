/**
 * @file cvedix_ff_des_node.h
 * @brief FFmpeg-based universal destination node for file/network output
 * 
 * High-performance video output using FFmpeg with hardware acceleration support.
 * 
 * @section ff_des_outputs Supported Outputs
 * - Local files: `./output.mp4`, `./output.mkv`
 * - RTMP streams: `rtmp://server/live/stream`
 * - RTSP streams, UDP, RTP
 * 
 * @section ff_des_encoders Supported Encoders
 * - `libx264`, `libx265` (CPU)
 * - `h264_nvenc`, `hevc_nvenc` (NVIDIA GPU)
 * - `h264_vaapi`, `hevc_vaapi` (Intel GPU)
 * 
 * @section ff_des_prereq Prerequisites
 * - Compile with `-DCVEDIX_WITH_FFMPEG`
 * - FFmpeg with required encoder support
 * 
 * @see ff_des Low-level FFmpeg wrapper
 * @see cvedix_des_node Base class
 */

#pragma once
#ifdef CVEDIX_WITH_FFMPEG
#include "ff_des.h"
#include "cvedix/nodes/common/cvedix_des_node.h"

namespace cvedix_nodes {
    /**
     * @brief Universal destination node using FFmpeg
     * 
     * Supports file streams and network protocols (RTMP, RTSP, etc.).
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_ff_des_node final: public cvedix_des_node {

    private:
        /* inner members. */
        std::string m_out_uri = "";
        bool m_use_osd = true;
        int m_out_bitrate = 1024;
        std::string m_encoder_name = "";
        cvedix_objects::cvedix_size m_resolution_w_h;

        /**
         * encode & enmux.
         */
        ff_des_ptr m_ff_des = nullptr;
        /**
         * SwsContext used fot scale by FFmpeg.
         */
        SwsContext* sws_ctx = NULL;
    protected:
        // re-implementation, return nullptr.
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 
    public:
        /**
         * create cvedix_ff_des_node instance using initial parameters.
         * 
         * @param node_name specify the name of DES node.
         * @param channel_index specify the channel index of DES node.
         * @param out_uri specify the uri to be written.
         * @param use_osd specify use osd frame as output or not.
         * @param out_width specify the final width of output, 0 means use the width of frame flowing in pipeline.
         * @param out_height specify the final height of output, 0 means use the height of frame flowing in pipeline.
         * @param out_fps specify the fps of output, 0 means use the fps of original stream in pipeline.
         * @param out_bitrate specify the bitrate of output (kbit/s).
         * @param out_max_b_frames specify the max B frames in a GOP for encoding.
         * @param encoder_name specify the encoder name (`libx264`/`libx265`/`h264_nvenc`/`hevc_nvenc`) used for encoding in FFmpeg.
         * @param out_sw_pix_fmt specify the pixel format of output.
         * 
         * @note
         * the encoder specified by `encoder_name` MUST be supported already in FFmpeg, 
         * we can run `ffmpeg -encoders` to show list of encoders supported in FFmpeg.
         * if the encoder not found, please reconfigure & rebuild your FFmpeg.
         */
        cvedix_ff_des_node(const std::string& node_name,
                              int channel_index,
                              const std::string& out_uri,
                              cvedix_objects::cvedix_size resolution_w_h = {}, 
                              int bitrate = 1024,
                              bool osd = true,
                              std::string encoder_name = "libx264");
        ~cvedix_ff_des_node();

        /**
         * return out uri of DES node.
         */
        virtual std::string to_string() override;
    };
}
#endif