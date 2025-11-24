

#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include "../common/cvedix_des_node.h"

namespace cvedix_nodes {
    // screen des node, display video on local window.
    class cvedix_screen_des_node: public cvedix_des_node
    {
    private:
        /* data */
        const std::string base_gst_template = "appsrc ! videoconvert ! videoscale ! textoverlay text=%s halignment=left valignment=top font-desc='Sans,16' shaded-background=true ! timeoverlay halignment=right valignment=top font-desc='Sans,16' shaded-background=true ! queue ! %s";
        std::string gst_template;
        cv::VideoWriter screen_writer;
        std::string select_screen_sink(std::string node_name);
    protected:
        // re-implementation, return nullptr.
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 
        // re-implementation, return nullptr.
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_screen_des_node(std::string node_name, 
                            int channel_index, 
                            bool osd = true,
                            cvedix_objects::cvedix_size display_w_h = {});
        ~cvedix_screen_des_node();

        // for osd frame
        bool osd;
        // display size
        cvedix_objects::cvedix_size display_w_h;
    };
}