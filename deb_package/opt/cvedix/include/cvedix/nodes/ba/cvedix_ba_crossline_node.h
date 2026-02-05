#pragma once

#include <map>
#include "../common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"

namespace cvedix_nodes {
    // behaviour analysis node for crossline (support multi channels)
    class cvedix_ba_crossline_node: public cvedix_node 
    {
    private:
        // channel -> counter
        std::map<int, int> all_total_crossline;

        // channel -> line, 1 channel supports only 1 line at most (can be 0, which means no crossline check on this channel)
        std::map<int, cvedix_objects::cvedix_line> all_lines;

        // record params
        bool need_record_image;
        bool need_record_video;

        // check if point at one side of line
        bool at_1_side_of_line(cvedix_objects::cvedix_point p, cvedix_objects::cvedix_line line);
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_ba_crossline_node(std::string node_name, 
                            std::map<int, cvedix_objects::cvedix_line> lines,
                            bool need_record_image = true,
                            bool need_record_video = false);
        ~cvedix_ba_crossline_node();

        std::string to_string() override;
    };
}