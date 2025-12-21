/**
 * @file cvedix_lane_osd_node.h
 * @brief OSD for lane detection visualization
 * 
 * Draws detected lane lines on frame.
 */

#pragma once

#include <string>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Lane detection OSD visualization
     */
    class cvedix_lane_osd_node: public cvedix_node
    {

    private:
        /* data */
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_lane_osd_node(std::string node_name);
        ~cvedix_lane_osd_node();
    };
}