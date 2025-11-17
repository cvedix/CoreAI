

#include "cvedix_point.h"

namespace cvedix_objects {
    
    cvedix_point::cvedix_point(int x, int y): x(x), y(y) {

    }
    
    cvedix_point::~cvedix_point() {

    }

    float cvedix_point::distance_with(const cvedix_point & p) {
        return std::sqrt(std::pow(x-p.x, 2) + std::pow(y-p.y, 2));
    }
}