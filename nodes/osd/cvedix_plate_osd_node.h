#pragma once

#include <opencv2/freetype.hpp>

#include "cvedix/nodes/common/cvedix_node.h"


namespace cvedix_nodes {
    // on screen display(short as osd) node.
    // used for displaying vehicle plate on frame, draw rectangle according to plate color
    class cvedix_plate_osd_node: public cvedix_node
    {
    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;
        float mask_threshold = 0.3;

        // draw color on frame
        std::map<std::string, cv::Scalar> draw_colors {{"blue", cv::Scalar(255, 0, 0)},
                                                {"green", cv::Scalar(0, 255, 0)},
                                                {"yellow", cv::Scalar(0, 255, 255)},
                                                {"white", cv::Scalar(255, 255, 255)}};

        // map to Chinese
        std::map<std::string, std::string> text_colors {{"blue", "蓝"},
                                                {"green", "绿"},
                                                {"yellow", "黄"},
                                                {"white", "白"}};
        
        // history plates at the bottom of screen 
        std::vector<cv::Mat> plates_his;
        int height_his = 100;
        int gap_his = 10;
        bool display_his = true;

    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_plate_osd_node(std::string node_name, std::string font = "", bool display_his = true);
        ~cvedix_plate_osd_node();
    };
}