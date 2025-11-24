#pragma once

#include <list>
#include <deque>
#include <map>

#include "../common/cvedix_node.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"

#include "cvedix_image_record_task.h"
#include "cvedix_video_record_task.h"
#include "cvedix_record_status_hookable.h"

namespace cvedix_nodes {
    // video/image recording node, save it to local disk.
    // it is a middle node but works asynchronously, so recording would not block the pipeline.
    // note record node could work on multi channels at the same time.
    class cvedix_record_node: public cvedix_node, public cvedix_record_status_hookable
    {
    private:
        /* config data */
        // video save directory
        std::string video_save_dir;
        // image save directory
        std::string image_save_dir;
        // pre record time for video (seconds)
        int pre_record_video_duration;
        // record time for video (seconds), not including pre_record_video_duration
        int record_video_duration;
        // auto create sub directory by date and channel or not, such as `./video_save_dir/2022-10-8/1/**.mp4`
        bool auto_sub_dir;
        // width and height
        cvedix_objects::cvedix_size resolution_w_h = {};
        // bitrate for video record
        int bitrate = 1024;
        // record osd frame or not
        bool osd = false;

        // fps for current video
        // int fps = 0;
        std::map<int, int> all_fps;
        
        /* record task list */
        // std::list<std::shared_ptr<cvedix_nodes::cvedix_record_task>> record_tasks;
        std::map<int, std::list<std::shared_ptr<cvedix_nodes::cvedix_record_task>>> all_record_tasks;

        /* pre-record for video */
        // std::deque<std::shared_ptr<cvedix_objects::cvedix_frame_meta>> pre_records;
        std::map<int, std::deque<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>> all_pre_records;

        // new record task
        void auto_new_record_task(std::shared_ptr<cvedix_objects::cvedix_control_meta>& meta);

    protected:
        // re-implementation
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 
        // re-implementation
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_record_node(std::string node_name, 
                        std::string video_save_dir, 
                        std::string image_save_dir,
                        cvedix_objects::cvedix_size resolution_w_h = {}, 
                        bool osd = false,
                        int pre_record_video_duration = 5, 
                        int record_video_duration = 20,
                        bool auto_sub_dir = true,
                        int bitrate = 1024);
        ~cvedix_record_node();
    };
}
