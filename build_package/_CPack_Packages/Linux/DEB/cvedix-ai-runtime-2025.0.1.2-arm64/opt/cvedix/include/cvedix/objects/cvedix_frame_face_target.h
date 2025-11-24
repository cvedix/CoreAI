
#pragma once
#include <vector>
#include <memory>
#include "shapes/cvedix_rect.h"


namespace cvedix_objects {
    // target in frame detected by face detectors such as yunet.
    // note: we can define new target type like cvedix_frame_xxx_target... if need (see cvedix_frame_pose_target also)
    class cvedix_frame_face_target
    {
    private:
        /* data */
    public:
        cvedix_frame_face_target(int x, 
                            int y, 
                            int width, 
                            int height, 
                            float score, 
                            std::vector<std::pair<int, int>> key_points = std::vector<std::pair<int, int>>(), 
                            std::vector<float> embeddings = std::vector<float>());
        ~cvedix_frame_face_target();

        // x of top left
        int x;
        // y of top left
        int y;
        // width of rect
        int width;
        // height of rect
        int height;

        // confidence
        float score;

        // feature vector created by infer nodes such as cvedix_sface_feature_encoder_node.
        // embeddings can be used for face recognize or other reid works.
        std::vector<float> embeddings;

        // key points (5 points or more)
        std::vector<std::pair<int, int>> key_points;

        // track id filled by cvedix_track_node (child class) if it exists.
        int track_id = -1;
        // cache of track rects in the previous frames, filled by track node if it exists. 
        // we can draw / analyse depend on these track rects later.
        std::vector<cvedix_objects::cvedix_rect> tracks;
        
        // clone myself
        std::shared_ptr<cvedix_frame_face_target> clone();

        // rect area of target
        cvedix_rect get_rect() const;
    };

}