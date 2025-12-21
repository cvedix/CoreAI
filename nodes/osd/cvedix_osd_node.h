/**
 * @file cvedix_osd_node.h
 * @brief On-Screen Display node for drawing detection results
 * 
 * Base OSD node for displaying cvedix_frame_target on frames.
 * Supports Chinese/Unicode fonts via FreeType.
 * 
 * @section osd_purpose Purpose
 * - Debug: Visual verification of inference results
 * - Demo: Screen casting to showcase product capabilities
 */

#pragma once

#include <opencv2/freetype.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {

    /** @brief OSD configuration options */
    typedef struct cvedix_osd_node_option
    {
        int aaa = 0;
    } cvedix_osd_option;
    
    /**
     * @brief Base OSD node for target visualization
     */
    class cvedix_osd_node: public cvedix_node {

    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_osd_node(std::string node_name, std::string font = "");
        ~cvedix_osd_node();
    };

}