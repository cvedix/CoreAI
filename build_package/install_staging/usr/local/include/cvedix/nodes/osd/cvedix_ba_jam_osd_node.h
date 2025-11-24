#pragma once

#include <map>
#include <opencv2/freetype.hpp>
#include "../common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_line.h"

namespace cvedix_nodes {
    // osd node for behaviour analysis of stop
    class cvedix_ba_jam_osd_node: public cvedix_node
    {
    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;

        // support multi channels
        std::map<int, std::vector<cvedix_objects::cvedix_point>> all_jam_regions;   // channel -> jam region
        std::map<int, bool> all_jam_results;  // channel -> jam status
        std::map<int, std::vector<int>> all_involve_ids;  // channel -> target ids when enter jam status
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_ba_jam_osd_node(std::string node_name,  std::string font = "");
        ~cvedix_ba_jam_osd_node();
    };
}