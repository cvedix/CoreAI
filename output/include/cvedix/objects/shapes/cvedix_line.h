

#pragma once

#include "cvedix_point.h"

namespace cvedix_objects {
    // line in 2-dims coordinate system
    class cvedix_line {
    private:
        /* data */
    public:
        cvedix_line() = default;
        cvedix_line(cvedix_point start, cvedix_point end);
        ~cvedix_line();

        cvedix_point start;
        cvedix_point end;

        // distance between start and end point
        float length();
    };

}