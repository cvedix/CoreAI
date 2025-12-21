/**
 * @file cvedix_text_osd_node.h
 * @brief OSD for OCR text detection results
 * 
 * Draws cvedix_frame_text_target bounding boxes and recognized text.
 */

#pragma once

#include <opencv2/imgproc.hpp>
#include <opencv2/freetype.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief OCR text OSD visualization
     */
    class cvedix_text_osd_node: public cvedix_node
    {

    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_text_osd_node(std::string node_name, std::string font);
        ~cvedix_text_osd_node();
    };

}