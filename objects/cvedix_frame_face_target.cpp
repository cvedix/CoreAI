#include "cvedix_frame_face_target.h"

namespace cvedix_objects {
        
    cvedix_frame_face_target::cvedix_frame_face_target(int x, 
                                                int y, 
                                                int width, 
                                                int height, 
                                                float score, 
                                                std::vector<std::pair<int, int>> key_points, 
                                                std::vector<float> embeddings):
                                                x(x),
                                                y(y),
                                                width(width),
                                                height(height),
                                                score(score),
                                                key_points(key_points),
                                                embeddings(embeddings) {
        
    }
    
    cvedix_frame_face_target::~cvedix_frame_face_target() {
    }

    std::shared_ptr<cvedix_frame_face_target> cvedix_frame_face_target::clone() {
        return std::make_shared<cvedix_frame_face_target>(*this);
    }

    cvedix_rect cvedix_frame_face_target::get_rect() const{
        return cvedix_rect(x, y, width, height);
    }
}