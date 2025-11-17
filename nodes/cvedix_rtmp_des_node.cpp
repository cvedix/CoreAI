
#include <assert.h>
#include "cvedix_rtmp_des_node.h"
#include "../utils/cvedix_utils.h"

namespace cvedix_nodes {
        
    cvedix_rtmp_des_node::cvedix_rtmp_des_node(std::string node_name, 
                                        int channel_index, 
                                        std::string rtmp_url, 
                                        cvedix_objects::cvedix_size resolution_w_h, 
                                        int bitrate,
                                        bool osd,
                                        std::string gst_encoder_name):
                                        cvedix_des_node(node_name, channel_index),
                                        rtmp_url(rtmp_url),
                                        resolution_w_h(resolution_w_h),
                                        bitrate(bitrate),
                                        osd(osd),
                                        gst_encoder_name(gst_encoder_name) {
        // append channel_index to the end of rtmp_url.
        // if original rtmp_url is rtmp://192.168.77.105/live/10000 and channel_index is 10
        // then the modified rtmp_url is rtmp://192.168.77.105/live/10000_10
        this->rtmp_url = this->rtmp_url + "_" + std::to_string(channel_index);
        this->gst_template = cvedix_utils::string_format(this->gst_template, gst_encoder_name.c_str(), bitrate, this->rtmp_url.c_str());
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), gst_template.c_str()));
        this->initialized();
    }
    
    cvedix_rtmp_des_node::~cvedix_rtmp_des_node() {
        deinitialized();        
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_rtmp_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));
            
            cv::Mat resize_frame;
            if (this->resolution_w_h.width != 0 && this->resolution_w_h.height != 0) {                 
                cv::resize((osd && !meta->osd_frame.empty()) ? meta->osd_frame : meta->frame, resize_frame, cv::Size(resolution_w_h.width, resolution_w_h.height));
            }
            else {
                resize_frame = (osd && !meta->osd_frame.empty()) ? meta->osd_frame : meta->frame;
            }

            if (!rtmp_writer.isOpened()) {
                assert(rtmp_writer.open(this->gst_template, cv::CAP_GSTREAMER, 0, meta->fps, {resize_frame.cols, resize_frame.rows}));
            }

            rtmp_writer.write(resize_frame);

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