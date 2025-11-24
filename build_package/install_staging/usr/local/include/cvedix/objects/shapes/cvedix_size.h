
#pragma once

#include <utility>

namespace cvedix_objects {
    // size(width and height) in 2-dims coordinate system
    class cvedix_size {
    private:
        /* data */
    public:
        cvedix_size(int width = 0, int height = 0);
        ~cvedix_size();


        int width;
        int height;
    };

}