/**
 * @file cvedix_placeholder_node.h
 * @brief Placeholder node for pipeline structure
 * 
 * Does nothing, just passes data through. Useful for pipeline topology design.
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Placeholder node - passes data through unchanged
     */
    class cvedix_placeholder_node: public cvedix_node {

    private:
        /* data */
    public:
        cvedix_placeholder_node(std::string node_name);
        ~cvedix_placeholder_node();
    };

}