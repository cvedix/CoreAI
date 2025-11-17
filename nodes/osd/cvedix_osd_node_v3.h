#pragma once

#include <opencv2/freetype.hpp>

#include "../cvedix_node.h"


namespace cvedix_nodes {
    // on screen display(short as osd) node.
    // another version for cvedix_frame_target display, display mask area for image segmentation.
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