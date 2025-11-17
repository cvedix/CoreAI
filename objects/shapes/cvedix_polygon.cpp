
#include <assert.h>
#include "cvedix_polygon.h"

namespace cvedix_objects {
        
    cvedix_polygon::cvedix_polygon(std::vector<cvedix_point> vertexs): vertexs(vertexs) {
        assert(vertexs.size() > 2);
    }
    
    cvedix_polygon::~cvedix_polygon() {

    }
    
    bool cvedix_polygon::contains(const cvedix_point & p) {
        return true;
    }
}