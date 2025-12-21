/**
 * @file cvedix_osd_node_v2.h
 * @brief OSD v2 - displays sub-targets at screen bottom
 * 
 * Shows cvedix_sub_target thumbnails in a bottom panel.
 */

#pragma once

#include <opencv2/freetype.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief OSD v2 with bottom panel for sub-targets
     */
    class cvedix_osd_node_v2: public cvedix_node
    {

    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;
                
        // leave a gap at the bottom of osd frame
        int gap_height = 256;
        int padding = 10;
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_osd_node_v2(std::string node_name, std::string font = "");
        ~cvedix_osd_node_v2();
    };

}