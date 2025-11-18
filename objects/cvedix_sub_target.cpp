
#include "cvedix_sub_target.h"

namespace cvedix_objects {
    
    cvedix_sub_target::cvedix_sub_target(int x, 
                        int y, 
                        int width, 
                        int height, 
                        int class_id, 
                        float score, 
                        std::string label, 
                        int frame_index, 
                        int channel_index):
                        x(x),
                        y(y),
                        width(width),
                        height(height),
                        class_id(class_id),
                        score(score),
                        label(label),
                        frame_index(frame_index),
                        channel_index(channel_index) {
    }
    
    cvedix_sub_target::~cvedix_sub_target() {
    }    

    std::shared_ptr<cvedix_sub_target> cvedix_sub_target::clone() {
        return std::make_shared<cvedix_sub_target>(*this);
    }

    cvedix_rect cvedix_sub_target::get_rect() const {
        return cvedix_rect(x, y, width, height);
    }
}