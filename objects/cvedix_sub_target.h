/**
 * @file cvedix_sub_target.h
 * @brief Sub-target within a parent target
 * 
 * Detections on cropped images (e.g., plate on vehicle).
 */

#pragma once

#include <string>
#include <memory>
#include <vector>

#include "shapes/cvedix_rect.h"

namespace cvedix_objects {
    /**
     * @brief Sub-target within parent target
     */
    class cvedix_sub_target
    {

    private:
        /* data */
    public:
        cvedix_sub_target(int x, 
                        int y, 
                        int width, 
                        int height, 
                        int class_id, 
                        float score, 
                        std::string label, 
                        int frame_index, 
                        int channel_index);
        ~cvedix_sub_target();

        // x of top left
        int x;
        // y of top left
        int y;
        // width of rect
        int width;
        // height of rect
        int height;

        // class id
        int class_id;
        // score
        float score;
        // label
        std::string label;

        // frame the sub target belongs to
        int frame_index;
        // channel the sub target belongs to
        int channel_index;

        // save some info
        std::vector<std::string> attachments;

        // clone myself
        std::shared_ptr<cvedix_sub_target> clone();

        // rect area of target
        cvedix_rect get_rect() const;
    };

}