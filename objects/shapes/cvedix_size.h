/**
 * @file cvedix_size.h
 * @brief 2D size (width/height) geometry primitive
 */

#pragma once

#include <utility>

namespace cvedix_objects {
    /**
     * @brief 2D size (width, height)
     */
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