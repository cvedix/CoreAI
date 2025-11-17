
#include "cvedix_fake_des_node.h"

namespace cvedix_nodes {
        
    cvedix_fake_des_node::cvedix_fake_des_node(std::string node_name, 
                        int channel_index): cvedix_des_node(node_name, channel_index) {
        this->initialized();
    }
    
    cvedix_fake_des_node::~cvedix_fake_des_node() {
        deinitialized();
    }
}