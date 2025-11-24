
#pragma once

#include "cvedix_record_task.h"

namespace cvedix_nodes {
    // image record task, each task instance responsible for recording only 1 image file.
    // create multi instances if multi images need to be record at the same time, and maintain these tasks in a list.
    // note, the cost of image record is very low but still let it work asynchronously (derived from cvedix_record_task).
    class cvedix_image_record_task: public cvedix_record_task {
    private:
    protected:
        // define how to record image
        virtual void record_task_run() override;
        // retrive .jpg as file extension
        virtual std::string get_file_ext() override;
    public:
        cvedix_image_record_task(int channel_index,
                            std::string file_name_without_ext,
                            std::string save_dir,
                            bool auto_sub_dir,
                            bool osd,
                            cvedix_objects::cvedix_size resolution_w_h,
                            std::string host_node_name = "host_node_not_specified",
                            bool auto_start = true);
        ~cvedix_image_record_task();
    };
}