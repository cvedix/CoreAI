#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/core.hpp>

#include "shapes/cvedix_rect.h"
#include "cvedix_sub_target.h"

/*
* ##################################################
* what is frame target? 
* ##################################################
* frame target are those detected by deep learning models(detectors) and then updated by other classifiers.
* we can detect vehicles, pedestrain, traffic lights, firesmoke and so on using cvedix_primary_infer_node, and then figure out what color the vehicles are, if the pedstrain wear a hat or not using cvedix_secondary_infer_node.
* vehicles, pedstrain are frame targets detected in current frame.
* 
* note:
* frame target is an important concept and it contains a lot of data which would be updated/filled by cvedix_node when flowing through the piepline.
* see cvedix_frame_meta also.
* ##################################################
*/

namespace cvedix_objects {
    // target in frame, detected by detectors(such as yolo/ssd).
    class cvedix_frame_target {
    private:
        /* data */
    public:
        // x of top left
        int x;
        // y of top left
        int y;
        // width of rect
        int width;
        // height of rect
        int height;

        // class id created by primary infer nodes.
        // allow multi primary infer nodes to exist in a pipeline, the class id is unique because we apply class_id_offset in each primary infer node.
        int primary_class_id;
        // score created by primary infer nodes
        float primary_score;
        // label created by primary infer nodes
        std::string primary_label;

        // frame the target belongs to
        int frame_index;
        // channel the target belongs to
        int channel_index;

        // track id created by track node if it exists
        int track_id = -1;
        // cache of track rects in the previous frames, filled by track node if it exists. 
        // we can draw / analyse depend on these track rects later.
        std::vector<cvedix_objects::cvedix_rect> tracks;

        // mask of the target, used for Instance Segmentation like mask rcnn network (ignore for other situations).
        cv::Mat mask;

        // class ids filled/appended by multi secondary infer nodes.
        std::vector<int> secondary_class_ids;
        // scores filled/appended by multi secondary infer nodes.
        std::vector<float> secondary_scores;
        // labels filled/appended by multi secondary infer nodes.
        std::vector<std::string> secondary_labels;
        
        // sub targets inside current target.
        // in case detectors applied on small cropped image.
        std::vector<std::shared_ptr<cvedix_objects::cvedix_sub_target>> sub_targets;

        // feature vector(for example, 128 or 256-dims array) created by infer nodes such as cvedix_feature_encoder_node.
        // each target has only one feature vector, the value will be override if multi cvedix_feature_encoder_node exist.
        // embeddings can be used for reid related works.
        std::vector<float> embeddings;

        // ba flags of the target, hold by this value (created/updated by ba nodes).
        // for example, 0001/0010/0100/1000 stands for 4 different flags, 1110 means 3 flags are on and another one is off, using ^|& operators to update and read. 
        // if 0100 stands for 'Stop' flag of target,  'ba_flags|=0100' means set 'Stop' flag as On, '(ba_flags & 0100) == 0100' means 'Stop' flag is already On. 
        int ba_flags;

        cvedix_frame_target(int x, 
                        int y, 
                        int width, 
                        int height, 
                        int primary_class_id, 
                        float primary_score, 
                        int frame_index, 
                        int channel_index,
                        std::string primary_label = "");
        cvedix_frame_target(cvedix_rect rect,
                        int primary_class_id, 
                        float primary_score, 
                        int frame_index, 
                        int channel_index,
                        std::string primary_label = "");
        ~cvedix_frame_target();

        // clone myself
        std::shared_ptr<cvedix_frame_target> clone();

        // rect area of target
        cvedix_rect get_rect() const;
    };
}