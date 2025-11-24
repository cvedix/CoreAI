
#pragma once

#include <memory>
#include <vector>
#include <string>

namespace cvedix_objects {
    class cvedix_frame_text_target
    {
    private:
        /* data */
    public:
        cvedix_frame_text_target(std::vector<std::pair<int, int>> region_vertexes, std::string text, float score);
        ~cvedix_frame_text_target();

        std::vector<std::pair<int, int>> region_vertexes;
        std::string text;
        float score;

        // flags for text
        std::string flags = "";
        // clone myself
        std::shared_ptr<cvedix_frame_text_target> clone();
    };
}