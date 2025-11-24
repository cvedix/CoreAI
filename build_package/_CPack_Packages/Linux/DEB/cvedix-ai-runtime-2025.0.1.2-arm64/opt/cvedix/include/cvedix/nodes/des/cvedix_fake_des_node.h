
#pragma once

#include "../common/cvedix_des_node.h"

namespace cvedix_nodes {
    // fake des node, do nothing just a placeholder
    class cvedix_fake_des_node: public cvedix_des_node {
    private:
        /* data */
    public:
        cvedix_fake_des_node(std::string node_name, 
                        int channel_index);
        ~cvedix_fake_des_node();
    };

}