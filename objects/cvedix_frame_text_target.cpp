
#include "cvedix_frame_text_target.h"

namespace cvedix_objects {
        
    cvedix_frame_text_target::cvedix_frame_text_target(std::vector<std::pair<int, int>> region_vertexes, 
                                                std::string text, 
                                                float score):
                                                region_vertexes(region_vertexes),
                                                text(text),
                                                score(score) {

    }
    
    cvedix_frame_text_target::~cvedix_frame_text_target() {
    
    }

    std::shared_ptr<cvedix_frame_text_target> cvedix_frame_text_target::clone() {
        return std::make_shared<cvedix_frame_text_target>(*this);
    }
}