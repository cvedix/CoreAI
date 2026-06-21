/**
 * @file cvedix_file_src_node.h
 * @brief Video file source node
 * 
 * Reads video from local file using GStreamer pipeline.
 */

#pragma once

#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "cvedix/nodes/common/cvedix_src_node.h"

namespace cvedix_nodes {
    /**
     * @brief File source node (local video)
     */
    class cvedix_file_src_node: public cvedix_src_node {

    private:
        /* data */
        std::string gst_template = "filesrc location=%s ! qtdemux ! h264parse ! %s ! videoconvert ! appsink sync=false";
        cv::VideoCapture file_capture;
    protected:
        // re-implemetation
        virtual void handle_run() override;
    public:
        cvedix_file_src_node(std::string node_name, 
                        int channel_index, 
                        std::string file_path, 
                        float resize_ratio = 1.0, 
                        bool cycle = true,
                        std::string gst_decoder_name = "avdec_h264",
                        int skip_interval = 0,
                        bool play_at_realtime = true);
        ~cvedix_file_src_node();

        virtual std::string to_string() override;
        std::string file_path;
        bool cycle;
    
        // set avdec_h264 as the default decoder, we can use hardware decoder instead.
        std::string gst_decoder_name = "avdec_h264";
        // 0 means no skip
        int skip_interval = 0;
        
        // if true, source will sleep to match video FPS. If false, process as fast as possible.
        bool play_at_realtime = true;
    };

}