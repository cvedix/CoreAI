
#include "cvedix_screen_des_node.h"
#include "../utils/cvedix_utils.h"

#include <cstdlib>
#include <cstring>

namespace cvedix_nodes {
    cvedix_screen_des_node::cvedix_screen_des_node(std::string node_name, 
                                            int channel_index, 
                                            bool osd,
                                            cvedix_objects::cvedix_size display_w_h):
                                            cvedix_des_node(node_name, channel_index),
                                            osd(osd),
                                            display_w_h(display_w_h) {
        auto sink = select_screen_sink(node_name);
        this->gst_template = cvedix_utils::string_format(this->base_gst_template.c_str(), node_name.c_str(), sink.c_str());
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), gst_template.c_str()));
        this->initialized();
    }
    
    cvedix_screen_des_node::~cvedix_screen_des_node() {
        deinitialized();
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_screen_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));
            
            cv::Mat resize_frame;
            if (this->display_w_h.width != 0 && this->display_w_h.height != 0) {                 
                cv::resize((osd && !meta->osd_frame.empty()) ? meta->osd_frame : meta->frame, resize_frame, cv::Size(display_w_h.width, display_w_h.height));
            }
            else {
                resize_frame = (osd && !meta->osd_frame.empty()) ? meta->osd_frame : meta->frame;
            }

            if (!screen_writer.isOpened()) {
                assert(screen_writer.open(this->gst_template, cv::CAP_GSTREAMER, 0, meta->fps, {resize_frame.cols, resize_frame.rows}));
            }
            screen_writer.write(resize_frame);

            // for general works defined in base class
            return cvedix_des_node::handle_frame_meta(meta);
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_screen_des_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
            // for general works defined in base class
            return cvedix_des_node::handle_control_meta(meta);
    }

    std::string cvedix_screen_des_node::select_screen_sink(std::string node_name) {
        const char* custom_sink = std::getenv("CVEDIX_SCREEN_SINK");
        if (custom_sink != nullptr && std::strlen(custom_sink) > 0) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] use custom screen sink: %s", node_name.c_str(), custom_sink));
            return custom_sink;
        }

        const char* display = std::getenv("DISPLAY");
        if (display != nullptr && std::strlen(display) > 0) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] detected DISPLAY=%s, use fpsdisplaysink+ximagesink", node_name.c_str(), display));
            return "fpsdisplaysink video-sink=ximagesink sync=false";
        }

        const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
        if (wayland_display != nullptr && std::strlen(wayland_display) > 0) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] detected WAYLAND_DISPLAY=%s, use waylandsink", node_name.c_str(), wayland_display));
            return "waylandsink sync=false";
        }

        CVEDIX_INFO(cvedix_utils::string_format("[%s] no DISPLAY detected, fallback to kmssink", node_name.c_str()));
        return "kmssink sync=false";
    }
}