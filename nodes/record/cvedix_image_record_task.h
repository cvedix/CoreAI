/**
 * @file cvedix_image_record_task.h
 * @brief Image recording task (single image)
 * 
 * Async task for saving single image to disk.
 */

#pragma once

#include "cvedix_record_task.h"

namespace cvedix_nodes {
    /**
     * @brief Async image recording task
     */
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