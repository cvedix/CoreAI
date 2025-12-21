/**
 * @file cvedix_record_task.h
 * @brief Base class for async recording tasks
 * 
 * Provides infrastructure for video/image recording with:
 * - Async thread execution
 * - Progress tracking
 * - Callback on completion
 */

#pragma once

#include <deque>
#include <thread>
#include <memory>
#include <functional>
#include <experimental/filesystem>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgcodecs.hpp>

#include "cvedix/objects/cvedix_frame_meta.h"
#include "cvedix/utils/cvedix_utils.h"
#include "cvedix/utils/cvedix_semaphore.h"
#include "cvedix/utils/logger/cvedix_logger.h"

namespace cvedix_nodes {
    /** @brief Record type enumeration */
    enum cvedix_record_type {
        IMAGE,
        VIDEO
    };

    /** @brief Record information for callbacks */
    struct cvedix_record_info {
        int channel_index;
        cvedix_record_type record_type = cvedix_record_type::IMAGE;
        std::string file_name_without_ext;
        std::string full_record_path;
        bool osd;
        int pre_record_video_duration = 0;
        int record_video_duration = 0;
    };

    /** @brief Callback when recording completes */
    typedef std::function<void(int, cvedix_record_info)> cvedix_record_task_complete_hooker;

    /** @brief Task status enumeration */
    enum cvedix_record_task_status {
        NOSTRAT,   ///< Not started
        STARTED,   ///< Recording in progress
        COMPLETE   ///< Recording finished
    };

    /**
     * @brief Base class for recording tasks
     */
    class cvedix_record_task {

    private:
        int channel_index;
        std::string file_name_without_ext;
        std::string save_dir;
        bool auto_sub_dir;
        cvedix_objects::cvedix_size resolution_w_h;
        bool osd;
        
        std::string full_record_path = "";
        cvedix_record_task_complete_hooker task_complete_hooker;
    protected:
        // record thread
        std::thread record_task_th;
        // record thread func, implemented by child class
        virtual void record_task_run() = 0;
        // wait thread exit in cvedix_record_task
        void stop_task();
        // preprocess, choose frame type (osd or not) and resize
        void preprocess(std::shared_ptr<cvedix_objects::cvedix_frame_meta>& frame_to_record, cv::Mat& data);
        // get file extension override by specific class (for example, .mp4 for video and .jpg for image)
        virtual std::string get_file_ext() = 0;

        // notify to host when task complete
        void notify_task_complete(cvedix_record_info record_info);

        // cache frames to be recorded (video or image)
        // 1. include pre-record frames for video
        // 2. just one frame enough for image
        std::deque<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>  frames_to_record;

        // synchronize for cache
        cvedix_utils::cvedix_semaphore cache_semaphore;
        std::string host_node_name;   // the node name of host, cvedix_record_task is mainly used inside node.
    public:
        // status
        cvedix_record_task_status status = cvedix_record_task_status::NOSTRAT;
        // get full record path for file, include path, name with extension
        std::string get_full_record_path();
        // register hooker for recording complete
        void set_task_complete_hooker(cvedix_record_task_complete_hooker task_complete_hooker);
        // start task async
        void start();
   
        // append asynchronously, just write frame to cache
        void append_async(std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta);

        cvedix_record_task(int channel_index,
                        std::string file_name_without_ext, 
                        std::string save_dir, 
                        bool auto_sub_dir, 
                        cvedix_objects::cvedix_size resolution_w_h, 
                        bool osd,
                        std::string host_node_name);
        virtual ~cvedix_record_task();   // keep virtual since we need destruct child class via base pointer
    };

}