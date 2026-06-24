
#include <iostream>

#include <opencv2/core/utils/logger.hpp>

#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix_file_src_node.h"

namespace cvedix_nodes {
        
    cvedix_file_src_node::cvedix_file_src_node(std::string node_name, 
                                        int channel_index, 
                                        std::string file_path, 
                                        float resize_ratio, 
                                        bool cycle,
                                        std::string gst_decoder_name,
                                        int skip_interval,
                                        bool play_at_realtime): 
                                        cvedix_src_node(node_name, channel_index, resize_ratio), 
                                        file_path(file_path), 
                                        cycle(cycle), gst_decoder_name(gst_decoder_name), skip_interval(skip_interval), play_at_realtime(play_at_realtime) {
        assert(skip_interval >= 0 && skip_interval <= 9);
        this->gst_template = cvedix_utils::string_format(this->gst_template, file_path.c_str(), gst_decoder_name.c_str());
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), gst_template.c_str()));
        this->initialized();
    }
    
    cvedix_file_src_node::~cvedix_file_src_node() {
        deinitialized();
    }
    
    // define how to read video from local file, create frame meta etc.
    // please refer to the implementation of cvedix_node::handle_run.
    void cvedix_file_src_node::handle_run() {
        cv::Mat frame;
        int video_width = 0;
        int video_height = 0;
        int fps = 0;
        std::chrono::milliseconds delta;
        int skip = 0;

        while(alive) {
            // check if need work
            gate.knock();

            auto last_time = std::chrono::system_clock::now();
            // try to open capture
            if (!file_capture.isOpened()) {
                cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);
                bool opened = file_capture.open(this->gst_template, cv::CAP_GSTREAMER);
                cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);
                if(!opened) {
                    CVEDIX_WARN(cvedix_utils::string_format("[%s] open file failed, try again...", node_name.c_str()));
                    continue;
                }
            }

            // video properties
            if (video_width == 0 || video_height == 0 || fps == 0) {
                video_width = file_capture.get(cv::CAP_PROP_FRAME_WIDTH);
                video_height = file_capture.get(cv::CAP_PROP_FRAME_HEIGHT);
                fps = file_capture.get(cv::CAP_PROP_FPS);
                if (fps <= 0) {
                    CVEDIX_WARN(cvedix_utils::string_format("[%s] FPS query returned %d, defaulting to 25", node_name.c_str(), fps));
                    fps = 25;
                }
                delta = std::chrono::milliseconds(1000 / fps) * (skip_interval + 1);
    
                original_fps = fps;
                original_width = video_width;
                original_height = video_height;

                // set true fps because skip some frames
                fps = fps / (skip_interval + 1);
            }
            // stream_info_hooker activated if need
            cvedix_stream_info stream_info {channel_index, original_fps, original_width, original_height, to_string()};
            invoke_stream_info_hooker(node_name, stream_info);
            
            file_capture >> frame;
            if(frame.empty()) {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] reading frame complete, total frame==>%d", node_name.c_str(), frame_index));
                if (cycle) {
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] cycle flag is true, continue!", node_name.c_str()));
                    file_capture.set(cv::CAP_PROP_POS_FRAMES, 0);
                }
                continue;
            }

            // need skip
            if (skip < skip_interval) {
                skip++;
                continue;
            }
            skip = 0;

            // need resize
            cv::Mat resize_frame;
            if (this->resize_ratio != 1.0f) {                 
                cv::resize(frame, resize_frame, cv::Size(), resize_ratio, resize_ratio);
            }
            else {
                resize_frame = frame.clone();  // clone!
            }
            // set true size because resize
            video_width = resize_frame.cols;
            video_height = resize_frame.rows;

            this->frame_index++;
            // create frame meta
            auto out_meta = 
                std::make_shared<cvedix_objects::cvedix_frame_meta>(resize_frame, this->frame_index, this->channel_index, video_width, video_height, fps);

            if (out_meta != nullptr) {
                this->out_queue.push(out_meta);

                // handled hooker activated if need
                if (this->meta_handled_hooker) {
                    meta_handled_hooker(node_name, out_queue.size(), out_meta);
                }

                // important! notify consumer of out_queue in case it is waiting.
                this->out_queue_semaphore.signal();
                CVEDIX_DEBUG(cvedix_utils::string_format("[%s] after handling meta, out_queue.size()==>%d", node_name.c_str(), out_queue.size()));
            }

            // for fps
            if (play_at_realtime) {
                auto snap = std::chrono::system_clock::now() - last_time;
                snap = std::chrono::duration_cast<std::chrono::milliseconds>(snap);
                if (snap < delta) {
                    std::this_thread::sleep_for(delta - snap);
                }
            }
        }

        // send dead flag for dispatch_thread
        this->out_queue.push(nullptr);
        this->out_queue_semaphore.signal();        
    }

    // return stream path
    std::string cvedix_file_src_node::to_string() {
        return file_path;
    }
}
