
#include "cvedix_frame_pose_target.h"

namespace cvedix_objects {
    
    cvedix_frame_pose_target::cvedix_frame_pose_target(cvedix_pose_type type, 
                                                std::vector<cvedix_pose_keypoint> key_points):
                                                type(type),
                                                key_points(key_points) {

    }
    
    cvedix_frame_pose_target::~cvedix_frame_pose_target() {
    }

    std::shared_ptr<cvedix_frame_pose_target> cvedix_frame_pose_target::clone() {
        return std::make_shared<cvedix_frame_pose_target>(*this);
    }
}