
#pragma once

#include "../common/cvedix_node.h"

namespace cvedix_nodes {
    // placeholder node, do nothing just a placeholder in the middle of pipeline
    class cvedix_placeholder_node: public cvedix_node {
    private:
        /* data */
    public:
        cvedix_placeholder_node(std::string node_name);
        ~cvedix_placeholder_node();
    };

}