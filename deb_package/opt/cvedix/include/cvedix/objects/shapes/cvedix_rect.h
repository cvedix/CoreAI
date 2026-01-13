#pragma once

#include <tuple>

#include "cvedix_point.h"
#include "cvedix_size.h"

namespace cvedix_objects {
    // rect in 2-dims coordinate system
    class cvedix_rect {
    private:
        /* data */
    public:
        cvedix_rect() = default;
        cvedix_rect(int x, int y, int width, int height);
        cvedix_rect(cvedix_point left_top, cvedix_size wh);
        ~cvedix_rect();

        int x;
        int y;
        int width;
        int height;

        // get center point of the rect
        cvedix_point center();

        // get track point of the rect
        // track point is used to locate the target(represented by the rect)
        cvedix_point track_point();

        // calculate the iou with another rect
        float iou_with(const cvedix_rect & rect);

        // check if the rect contains a point
        bool contains(const cvedix_point & p);
    };

}