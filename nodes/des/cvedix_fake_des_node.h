/**
 * @file cvedix_fake_des_node.h
 * @brief Placeholder destination node (does nothing)
 * 
 * Empty destination node for testing or pipeline termination without output.
 * 
 * @section fake_des_usage Usage
 * @code
 * auto fake_des = std::make_shared<cvedix_fake_des_node>("null_sink", 0);
 * fake_des->attach_to({detector_node});
 * // Pipeline runs but produces no output
 * @endcode
 * 
 * @see cvedix_des_node Base class
 */

#pragma once

#include "cvedix/nodes/common/cvedix_des_node.h"

namespace cvedix_nodes {

    /**
     * @brief Placeholder destination node
     * 
     * Does nothing - used for testing or silent pipeline termination.
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_fake_des_node: public cvedix_des_node {
    private:
        /* data */

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param channel_index Channel index
         */
        cvedix_fake_des_node(std::string node_name, 
                        int channel_index);

        /// @brief Destructor
        ~cvedix_fake_des_node();
    };

}