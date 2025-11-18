#pragma once

#include <string>

#include "cvedix_node.h"
#include "cvedix_stream_status_hookable.h"

namespace cvedix_nodes {
    // base class for des nodes, end point of meta/pipeline.
    class cvedix_des_node: public cvedix_node, public cvedix_stream_status_hookable {
    private:
        // cache for stream status at current des node.
        cvedix_stream_status stream_status;

        // period(ms) to calculate output fps
        int fps_epoch = 500;
        int fps_counter = 0;
        std::chrono::system_clock::time_point fps_last_time;
    protected:
        // do nothing in des nodes
        virtual void dispatch_run() override final;
        // sample implementation, return nullptr in all des nodes.
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 
        // sample implementation, return nullptr in all des nodes.
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

        // protected as it can't be instanstiated directly.        
        cvedix_des_node(std::string node_name, int channel_index);
    public:
        ~cvedix_des_node();

        virtual cvedix_node_type node_type() override;

        int channel_index;
    };
}