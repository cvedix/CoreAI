/**
 * @file cvedix_osd_node_v3.h
 * @brief OSD v3 - displays segmentation masks
 * 
 * Visualizes instance segmentation masks on frames.
 */

#pragma once

#include <opencv2/freetype.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief OSD v3 with segmentation mask overlay
     */
    class cvedix_osd_node_v3: public cvedix_node
    {

    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;
        float mask_threshold = 0.3;
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_osd_node_v3(std::string node_name, std::string font = "");
        ~cvedix_osd_node_v3();
    };
}