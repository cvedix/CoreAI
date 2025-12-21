/**
 * @file cvedix_polygon.h
 * @brief 2D polygon geometry primitive
 */

#pragma once

#include <vector>
#include "cvedix_point.h"

namespace cvedix_objects {
    /**
     * @brief 2D polygon with point containment check
     */
    class cvedix_polygon
    {

    private:
        /* data */
    public:
        cvedix_polygon() = default;
        cvedix_polygon(std::vector<cvedix_point> vertexs);
        ~cvedix_polygon();

        // vertexs of the polygon
        std::vector<cvedix_point> vertexs;

        // check if the polygon contains a point
        bool contains(const cvedix_point & p);
    };

}