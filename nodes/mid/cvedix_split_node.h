/**
 * @file cvedix_split_node.h
 * @brief Pipeline splitting with configurable behavior
 * 
 * Splits pipeline into multiple branches with options:
 * - split_with_channel_index: Route by channel index
 * - split_with_deep_copy: Deep copy meta for thread safety
 * 
 * @see cvedix_sync_node For merging split branches
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {

    /**
     * @brief Pipeline splitting node with configurable behavior
     * 
     * Unlike default node splitting (shallow copy, broadcast to all),
     * this node provides:
     * - Channel-based routing
     * - Deep copy for thread safety
     */
    class cvedix_split_node: public cvedix_node
    {

    private:
        /* data */
    protected:
        // re-implement how to push meta to next nodes.
        virtual void push_meta(std::shared_ptr<cvedix_objects::cvedix_meta> meta) override;
    public:
        cvedix_split_node(std::string node_name, bool split_with_channel_index = false, bool split_with_deep_copy = false);
        ~cvedix_split_node();

        bool split_with_channel_index;
        bool split_with_deep_copy;
    };

}