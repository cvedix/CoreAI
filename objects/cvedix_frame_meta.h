/**
 * @file cvedix_frame_meta.h
 * @brief Frame metadata containing targets and analysis results
 * 
 * Core data structure holding all frame-related information:
 * frame data, detected targets, tracking info, BA results.
 * 
 * @see cvedix_meta Base class
 * @see cvedix_frame_target Detection results
 */

#pragma once

#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/videoio.hpp>

#include "cvedix_meta.h"
#include "cvedix_frame_target.h"
#include "cvedix_frame_pose_target.h"
#include "cvedix_frame_face_target.h"
#include "cvedix_frame_text_target.h"
#include "ba/cvedix_ba_result.h"

namespace cvedix_objects {
    /**
     * @brief Frame metadata with targets and analysis results
     */
    class cvedix_frame_meta: public cvedix_meta {

    private:
        /* data */
    public:
        cvedix_frame_meta(cv::Mat frame, int frame_index = -1, int channel_index = -1, int original_width = 0, int original_height = 0, int fps = 0);
        ~cvedix_frame_meta();

        // define copy constructor since we need deep copy operation.
        cvedix_frame_meta(const cvedix_frame_meta& meta);

        // frame the meta belongs to, filled by src nodes.
        int frame_index;

        // fps for current video.
        int fps;

        // orignal frame width, fiiled by src nodes.
        int original_width;
        // original frame height, filled by src nodes.
        int original_height;

        // image data the meta holds, filled by src nodes.
        // deep copy needed here for this member.
        cv::Mat frame;

        // osd image data the meta holds, filled by osd node if exists.
        // deep copy needed here for this member.
        cv::Mat osd_frame;

        // mask for the WHOLE frame, filled by Semantic Segmentation nodes if exists.
        // deep copy needed here for this member.
        cv::Mat mask;

        // text description for frame (output from LLM)
        std::string description;

        // targets created/appended by primary infer nodes, and then updated by secondary infer nodes if exist.
        // it is shared_ptr<...> type just to keep same as elements.
        // deep copy needed here for this member.
        std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_target>> targets;

        // pose targets created/appened by primary infer nodes.
        std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_pose_target>> pose_targets;

        // face targets created/appened by primary infer nodes.
        std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_face_target>> face_targets;

        // text targets created/appened by primary infer nodes.
        std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_text_target>> text_targets;

        // ba results created/appened by ba nodes.
        std::vector<std::shared_ptr<cvedix_objects::cvedix_ba_result>> ba_results;
        
        // get target ptrs by target ids in current frame, ONLY supports cvedix_frame_target
        std::vector<std::shared_ptr<cvedix_frame_target>> get_targets_by_ids(const std::vector<int>& ids);

        // copy myself
        virtual std::shared_ptr<cvedix_meta> clone() override;
    };

}