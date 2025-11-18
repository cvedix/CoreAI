#pragma once

#include <string>
#include "../cvedix_node.h"

namespace cvedix_nodes {
    class cvedix_seg_osd_node: public cvedix_node
    {
    private:
        /* data */
        int gap = 60;
        
        // classs names of semantic segmentation
        std::vector<std::string> classes;
        // colors of semantic segmentation
        std::vector<cv::Vec3b> colors;
        void colorizeSegmentation(const cv::Mat &score, cv::Mat &segm);
        void showLegend(cv::Mat& board);

    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_seg_osd_node(std::string node_name, std::string classes_file = "", std::string colors_file = "");
        ~cvedix_seg_osd_node();
    };
}