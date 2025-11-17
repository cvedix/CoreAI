

#include "cvedix_rect.h"

namespace cvedix_objects {
        
    cvedix_rect::cvedix_rect(int x, int y, int width, int height): 
        x(x), 
        y(y), 
        width(width), 
        height(height) {

    }
    
    cvedix_rect::cvedix_rect(cvedix_point left_top, cvedix_size wh):
        x(left_top.x), y(left_top.y), width(wh.width), height(wh.height) {

    }


    cvedix_rect::~cvedix_rect() {

    }
    
    cvedix_point cvedix_rect::center() {
        return cvedix_point(x + width / 2, y + height / 2);
    }

    float cvedix_rect::iou_with(const cvedix_rect & rect) {
        return 1.0;
    }

    bool cvedix_rect::contains(const cvedix_point & p) {
        return true;
    }

    cvedix_point cvedix_rect::track_point() {
        // by default the center point of bottom is tracking point.
        return {x + width / 2, y + height};
    }
}