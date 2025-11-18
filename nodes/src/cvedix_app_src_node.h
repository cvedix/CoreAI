
#pragma once

#include "../common/cvedix_src_node.h"

namespace cvedix_nodes {
    // app src node, receive image data from external host code.
    class cvedix_app_src_node: public cvedix_src_node
    {
    private:
    protected:
        // just call cvedix_node::handle_run to ignore cvedix_src_node::handle_run
        virtual void handle_run() override;
        // just call cvedix_node::handle_frame_meta to ignore cvedix_src_node::handle_frame_meta
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 
        // just call cvedix_node::handle_control_meta to ignore cvedix_src_node::handle_control_meta
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

    public:
        cvedix_app_src_node(std::string node_name, 
                        int channel_index);
        ~cvedix_app_src_node();

        // push frames into pipeline
        // size of frame MUST be the same as the first time pushing to pipeline
        bool push_frames(std::vector<cv::Mat> frames);
    };
}