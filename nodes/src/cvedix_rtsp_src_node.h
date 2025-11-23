#pragma once

#include <string>

#include "../common/cvedix_src_node.h"

namespace cvedix_nodes {
    // rtsp source node, receive video stream via rtsp protocal.
    // example:
    // rtsp://admin:admin12345@192.168.77.110:554/
    class cvedix_rtsp_src_node: public cvedix_src_node {
    private:
        /* data */
        std::string gst_template = "rtspsrc location=%s ! application/x-rtp,media=video ! rtph264depay ! h264parse ! %s ! videoconvert ! appsink";
        cv::VideoCapture rtsp_capture;
    protected:
        // re-implemetation
        virtual void handle_run() override;
    public:
        cvedix_rtsp_src_node(std::string node_name, 
                        int channel_index, 
                        std::string rtsp_url, 
                        float resize_ratio = 1.0,
                        std::string gst_decoder_name = "avdec_h264",
                        int skip_interval = 0,
                        std::string codec_type = "h264");
        ~cvedix_rtsp_src_node();

        virtual std::string to_string() override;

        std::string rtsp_url;
        // set avdec_h264 as the default decoder, we can use hardware decoder instead.
        std::string gst_decoder_name = "avdec_h264";
        // 0 means no skip
        int skip_interval = 0;
        // h264, h265, auto
        std::string codec_type = "h264";
    };
}