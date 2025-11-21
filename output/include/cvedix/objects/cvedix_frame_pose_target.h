#pragma once

#include <vector>
#include <memory>

namespace cvedix_objects {

    // different types of datasets used to train openpose model.
    enum cvedix_pose_type {
        body_25,
        coco,
        mpi_15,
        face,
        hand,
        yolov8_pose_17
    };
    
    struct cvedix_pose_keypoint {
        int point_type;       // point type (index), nose, neck or left_eye 
        int x;                // x in 2D image
        int y;                // y in 2D image
        float score;          // probability
    };
    
    // target in frame detected by openpose(or other similar models), which mainly contains point collections.
    // note: we can define new target type like cvedix_frame_xxx_target... if need (see cvedix_frame_face_target also)
    class cvedix_frame_pose_target
    {
    private:
        /* data */
    public:
        cvedix_frame_pose_target(cvedix_pose_type type, std::vector<cvedix_pose_keypoint> key_points);
        ~cvedix_frame_pose_target();

        // target type, different models create different outputs which need specific parsing.
        cvedix_pose_type type;
        // keypoints array
        std::vector<cvedix_pose_keypoint> key_points;

        // clone myself
        std::shared_ptr<cvedix_frame_pose_target> clone();
    };
}