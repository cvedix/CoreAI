#pragma once

#include "cvedix_record_task.h"

namespace cvedix_nodes {
    // callback when record task complete.
    class cvedix_record_status_hookable
    {
    protected:
        cvedix_record_task_complete_hooker image_record_complete_hooker;
        cvedix_record_task_complete_hooker video_record_complete_hooker;
    public:
        cvedix_record_status_hookable(/* args */) {}
        ~cvedix_record_status_hookable() {}

        void set_image_record_complete_hooker(cvedix_record_task_complete_hooker image_record_complete_hooker) {
            this->image_record_complete_hooker = image_record_complete_hooker;
        }

        void set_video_record_complete_hooker(cvedix_record_task_complete_hooker video_record_complete_hooker) {
            this->video_record_complete_hooker = video_record_complete_hooker;
        }
    }; 
}