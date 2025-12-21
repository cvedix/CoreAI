/**
 * @file cvedix_expr_osd_node.h
 * @brief OSD for math expression display (OCR)
 * 
 * Draws cvedix_frame_text_target for math expressions.
 */

#pragma once

#include <opencv2/freetype.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Math expression OSD visualization
     */
    class cvedix_expr_osd_node: public cvedix_node
    {

    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_expr_osd_node(std::string node_name, std::string font);
        ~cvedix_expr_osd_node();
    };

}