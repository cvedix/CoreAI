
#include "cvedix_placeholder_node.h"

namespace cvedix_nodes {
        
    cvedix_placeholder_node::cvedix_placeholder_node(std::string node_name): cvedix_node(node_name) {
        this->initialized();
    }
    
    cvedix_placeholder_node::~cvedix_placeholder_node() {
        deinitialized();
    }
}