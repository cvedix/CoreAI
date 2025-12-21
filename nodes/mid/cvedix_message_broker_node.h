/**
 * @file cvedix_message_broker_node.h
 * @brief Generic message broker node for data forwarding
 * 
 * Forwards frame_meta and control_meta to downstream nodes.
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Generic message broker node
     */
    class cvedix_message_broker_node : public cvedix_node
    {

    private:
        /* data */
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_message_broker_node(std::string node_name);
        ~cvedix_message_broker_node();
    };    
}