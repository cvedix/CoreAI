/**
 * @file cvedix_rect.h
 * @brief 2D rectangle geometry primitive
 */

#pragma once

#include <tuple>
#include "cvedix_point.h"
#include "cvedix_size.h"

namespace cvedix_objects {

    /**
    * @brief Anchor point of rectangle enum
    */
    enum class cvedix_rect_anchor_point {
        CENTER,
        MID_TOP,
        MID_BOTTOM,
        MID_LEFT,
        MID_RIGHT,
        LEFT_TOP,
        RIGHT_TOP,
        LEFT_BOTTOM,
        RIGHT_BOTTOM
    };

    /**
     * @brief 2D rectangle
     */
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
        cvedix_point track_point( const cvedix_rect_anchor_point anchor = cvedix_rect_anchor_point::CENTER);

        // calculate the iou with another rect
        float iou_with(const cvedix_rect & rect);

        // check if the rect contains a point
        bool contains(const cvedix_point & p);
    };

}