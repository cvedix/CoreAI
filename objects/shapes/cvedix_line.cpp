

#include "cvedix_line.h"

namespace cvedix_objects {
    cvedix_line::cvedix_line(cvedix_point start, cvedix_point end): start(start), end(end) {

    }
    
    cvedix_line::~cvedix_line() {

    }

    float cvedix_line::length() {
        return start.distance_with(end);
    }
}