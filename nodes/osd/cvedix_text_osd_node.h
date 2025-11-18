
#pragma once
#include <opencv2/imgproc.hpp>
#include <opencv2/freetype.hpp>

#include "../cvedix_node.h"

namespace cvedix_nodes {
    // on screen display(short as osd) node.
    // mainly used to display cvedix_frame_text_target on frame.
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