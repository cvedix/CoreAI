/**
 * @file cvedix_ff_src_node.h
 * @brief FFmpeg-based universal source node for file/network input
 * 
 * High-performance video input using FFmpeg with hardware decoding support.
 * 
 * @section ff_src_inputs Supported Inputs
 * - Local files: `./video.mp4`, `./video.mkv`
 * - RTSP streams: `rtsp://camera/stream`
 * - HTTP streams, RTMP, RTP
 * 
 * @section ff_src_decoders Supported Decoders
 * - `h264`, `hevc` (CPU)
 * - `h264_cuvid`, `hevc_cuvid` (NVIDIA GPU)
 * - `h264_vaapi`, `hevc_vaapi` (Intel GPU)
 * 
 * @section ff_src_prereq Prerequisites
 * - FFmpeg with required decoder support (mandatory dependency)
 * 
 * @see ff_src Low-level FFmpeg wrapper
 * @see cvedix_src_node Base class
 */

#pragma once
#include "ff_src.h"
#include "cvedix/nodes/common/cvedix_src_node.h"

namespace cvedix_nodes {
    /**
     * @brief Universal source node using FFmpeg
     * 
     * Supports file streams and network protocols (RTSP, RTMP, HTTP).
     * 
     * @see cvedix_src_node Base class
     */
    class cvedix_ff_src_node final: public cvedix_src_node {

    private:
        /* inner members. */
        std::string m_decoder_name = "";
        std::string m_uri = "";
        // 0 means no skip
        int m_skip_interval = 0;

        /**
         * demux & decode.
         */
        ff_src_ptr m_ff_src = nullptr;
    protected:
        /**
         * get frames using FFmpeg.
         */
        virtual void handle_run() override;
    public:
        /**
         * create cvedix_ff_src_node instance using initial parameters.
         * 
         * @param node_name specify the name of SRC node.
         * @param channel_index specify the channel index of SRC node.
         * @param uri specify the uri to be opened by SRC node.
         * @param decoder_name specify the decoder name (`h264`/`hevc`/`h264_cuvid`/`hevc_cuvid`) used for decoding in FFmpeg.
         * @param resize_ratio specify the resize ratio applied to frames.
         * 
         * @note
         * the decoder specified by `decoder_name` MUST be supported already in FFmpeg, 
         * we can run `ffmpeg -decoders` to show list of decoders supported in FFmpeg.
         * if the decoder not found, please reconfigure & rebuild your FFmpeg.
         */
        cvedix_ff_src_node(const std::string& node_name, 
                              int channel_index,
                              const std::string& uri,
                              const std::string& decoder_name = "h264",
                              float resize_ratio = 1.0,
                              int skip_interval = 0);
        ~cvedix_ff_src_node();

        /**
         * return uri of SRC node.
         */
        virtual std::string to_string() override;
    };
}