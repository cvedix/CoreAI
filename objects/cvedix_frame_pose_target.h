/**
 * @file cvedix_frame_pose_target.h
 * @brief Human pose estimation target
 * 
 * Skeleton keypoints from OpenPose, YOLOv8-pose, etc.
 */

#pragma once

#include <vector>
#include <memory>

namespace cvedix_objects {

    /** @brief Pose dataset types */
    enum cvedix_pose_type {
        body_25,
        coco,
        mpi_15,
        face,
        hand,
        yolov8_pose_17
    };
    
    /** @brief Skeleton keypoint */
    struct cvedix_pose_keypoint {
        int point_type;
        int x;
        int y;
        float score;
    };
    
    /**
     * @brief Pose estimation target
     */
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