
#include "cvedix_message_broker_node.h"

namespace cvedix_nodes {
        
    cvedix_message_broker_node::cvedix_message_broker_node(std::string node_name): cvedix_node(node_name) {
        this->initialized();
    }
    
    cvedix_message_broker_node::~cvedix_message_broker_node() {

    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_message_broker_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_message_broker_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        /*
        if (meta->frame_index % 15 == 0) {          
            std::this_thread::sleep_for(std::chrono::milliseconds(28));
        }
        if (meta->frame_index % 73 == 0) {          
            std::this_thread::sleep_for(std::chrono::milliseconds(64));
        }*/
        return meta;
    }
}