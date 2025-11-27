
#pragma once

#include <utility>
#include <cmath>

namespace cvedix_objects {
    // point in 2-dims coordinate system
    class cvedix_point
    {
    private:
        /* data */
    public:
        cvedix_point(int x = 0, int y = 0);
        ~cvedix_point();

        int x;
        int y;

        // distance between 2 points
        float distance_with(const cvedix_point & p);
    };    
}