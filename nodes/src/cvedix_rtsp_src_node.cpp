
#ifdef CVEDIX_WITH_GSTREAMER
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <thread>
#include <chrono>

#include "cvedix_rtsp_src_node.h"
#include "cvedix/utils/cvedix_utils.h"

namespace cvedix_nodes {
        
    cvedix_rtsp_src_node::cvedix_rtsp_src_node(std::string node_name, 
                                        int channel_index, 
                                        std::string rtsp_url, 
                                        float resize_ratio,
                                        std::string gst_decoder_name,
                                        int skip_interval,
                                        std::string codec_type): 
                                        cvedix_src_node(node_name, channel_index, resize_ratio),
                                        rtsp_url(rtsp_url), gst_decoder_name(gst_decoder_name), skip_interval(skip_interval), codec_type(codec_type) {
        assert(skip_interval >= 0 && skip_interval <= 9);
        // use mpp decoder if available (Rockchip platform only)
        // Check if running on Rockchip platform (ARM64) and mppvideodec is available
        if (gst_decoder_name == "avdec_h264") {
            // Check architecture - mppvideodec only available on ARM64 (Rockchip)
            #ifdef __aarch64__
            // Check if mppvideodec plugin is available in GStreamer
            // Use popen instead of system() for better error handling
            std::string check_cmd = "gst-inspect-1.0 mppvideodec 2>&1 | head -1";
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(check_cmd.c_str(), "r"), pclose);
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    result += buffer.data();
                }
            }
            // If mppvideodec is available, result will contain "Factory Details"
            if (!result.empty() && result.find("Factory Details") != std::string::npos) {
            gst_decoder_name = "mppvideodec";
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Using mppvideodec (Rockchip hardware decoder)", node_name.c_str()));
            } else {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] mppvideodec not available, using avdec_h264 (software decoder)", node_name.c_str()));
            }
            #else
            // On x86_64/AMD64, always use software decoder (mppvideodec not available)
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Using avdec_h264 (software decoder) on x86_64 platform", node_name.c_str()));
            #endif
        }
        // Auto detection logic
        if (this->codec_type == "auto") {
            // Try to detect codec using gst-discoverer-1.0 command line tool
            // This avoids linking against gstreamer-pbutils and keeps dependencies simple
            std::string cmd = "gst-discoverer-1.0 -v " + rtsp_url + " | grep \"video/\" | head -n 1";
            std::array<char, 128> buffer;
            std::string result;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
            if (pipe) {
                while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                    result += buffer.data();
                }
            }
            
            if (result.find("h265") != std::string::npos || result.find("hevc") != std::string::npos) {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Auto-detected H265/HEVC stream", node_name.c_str()));
                this->codec_type = "h265";
            } else if (result.find("h264") != std::string::npos) {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Auto-detected H264 stream", node_name.c_str()));
                this->codec_type = "h264";
            } else {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Could not detect codec, falling back to decodebin", node_name.c_str()));
            }
        }

        // Build GStreamer pipeline with timeout and retry options for better connection handling
        // rtspsrc options:
        //   - latency=0: reduce latency
        //   - timeout=5000000000: 5 seconds timeout for connection
        //   - retry=1: enable retry on connection failure
        //   - protocols=tcp+udp: try both TCP and UDP
        std::string rtspsrc_options = "latency=0 timeout=5000000000 retry=1 protocols=tcp+udp";
        
        if (this->codec_type == "h265" || this->codec_type == "hevc") {
            this->gst_template = "rtspsrc " + rtspsrc_options + " location=%s ! application/x-rtp,media=video ! rtph265depay ! h265parse ! %s ! videoconvert ! appsink";
            this->gst_template = cvedix_utils::string_format(this->gst_template, rtsp_url.c_str(), gst_decoder_name.c_str());
        } else if (this->codec_type == "auto") {
            // Fallback if detection failed or returned unknown
            this->gst_template = "rtspsrc " + rtspsrc_options + " location=%s ! decodebin ! videoconvert ! appsink";
            this->gst_template = cvedix_utils::string_format(this->gst_template, rtsp_url.c_str());
        } else {
            // Default h264
            this->gst_template = "rtspsrc " + rtspsrc_options + " location=%s ! application/x-rtp,media=video ! rtph264depay ! h264parse ! %s ! videoconvert ! appsink";
            this->gst_template = cvedix_utils::string_format(this->gst_template, rtsp_url.c_str(), gst_decoder_name.c_str());
        }
        
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), gst_template.c_str()));
        this->initialized();
    }
    
    cvedix_rtsp_src_node::~cvedix_rtsp_src_node() {
        deinitialized();
    }
    
    // define how to read video from rtsp stream, create frame meta etc.
    // please refer to the implementation of cvedix_node::handle_run.
    void cvedix_rtsp_src_node::handle_run() {
        cv::Mat frame;
        int video_width = 0;
        int video_height = 0;
        int fps = 0;
        int skip = 0;
        int empty_frame_count = 0;
        const int MAX_EMPTY_FRAMES = 30; // Detect connection loss after 30 consecutive empty frames
        int retry_count = 0;
        const int MAX_RETRY_BEFORE_DELAY = 5;
        const int RETRY_DELAY_MS = 2000; // 2 seconds delay after multiple retries
        
        while(alive) {
            // check if need work
            gate.knock();
            
            // try to open capture or reconnect if connection lost
            if (!rtsp_capture.isOpened()) {
                video_width = video_height = fps = 0;
                original_width = original_height = original_fps = 0;
                empty_frame_count = 0;
                
                // Temporarily suppress OpenCV GStreamer warnings about frame count estimation
                // This is normal for RTSP streams as they don't have fixed frame counts
                cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);
                bool opened = rtsp_capture.open(this->gst_template, cv::CAP_GSTREAMER);
                cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);
                
                if (!opened) {
                    retry_count++;
                    if (retry_count >= MAX_RETRY_BEFORE_DELAY) {
                        CVEDIX_WARN(cvedix_utils::string_format("[%s] open rtsp failed %d times, waiting %d ms before retry...", 
                            node_name.c_str(), retry_count, RETRY_DELAY_MS));
                        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                        retry_count = 0;
                    } else {
                        CVEDIX_WARN(cvedix_utils::string_format("[%s] open rtsp failed, try again... (attempt %d)", 
                            node_name.c_str(), retry_count));
                        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Short delay between retries
                    }
                    continue;
                } else {
                    retry_count = 0;
                    CVEDIX_INFO(cvedix_utils::string_format("[%s] RTSP connection opened successfully", node_name.c_str()));
                }
            }

            // video properties
            if (video_width == 0 || video_height == 0 || fps == 0) {
                video_width = rtsp_capture.get(cv::CAP_PROP_FRAME_WIDTH);
                video_height = rtsp_capture.get(cv::CAP_PROP_FRAME_HEIGHT);
                fps = rtsp_capture.get(cv::CAP_PROP_FPS);
                
                original_fps = fps;
                original_width = video_width;
                original_height = video_height;

                // set true fps because skip some frames
                fps = fps / (skip_interval + 1);
            }
            // stream_info_hooker activated if need
            cvedix_stream_info stream_info {channel_index, original_fps, original_width, original_height, to_string()};
            invoke_stream_info_hooker(node_name, stream_info);

            rtsp_capture >> frame;
            if(frame.empty()) {
                empty_frame_count++;
                
                // Detect connection loss: if we get too many empty frames, close and reconnect
                if (empty_frame_count >= MAX_EMPTY_FRAMES) {
                    CVEDIX_WARN(cvedix_utils::string_format("[%s] Connection lost (empty frames: %d), closing and reconnecting...", 
                        node_name.c_str(), empty_frame_count));
                    rtsp_capture.release();
                    video_width = video_height = fps = 0;
                    original_width = original_height = original_fps = 0;
                    empty_frame_count = 0;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait before reconnect
                    continue;
                }
                
                if (empty_frame_count % 10 == 0) { // Log every 10 empty frames to avoid spam
                    CVEDIX_WARN(cvedix_utils::string_format("[%s] reading frame empty (count: %d/%d), total frame==>%d", 
                        node_name.c_str(), empty_frame_count, MAX_EMPTY_FRAMES, frame_index));
                }
                continue;
            }
            
            // Reset empty frame count on successful frame read
            empty_frame_count = 0;

            // need skip
            if (skip < skip_interval) {
                skip++;
                continue;
            }
            skip = 0;

            cv::Mat resize_frame;
            if (this->resize_ratio != 1.0f) {                 
                cv::resize(frame, resize_frame, cv::Size(), resize_ratio, resize_ratio);
            }
            else {
                resize_frame = frame.clone(); // clone!;
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
        }

        // send dead flag for dispatch_thread
        this->out_queue.push(nullptr);
        this->out_queue_semaphore.signal();    
    }

    // return stream url
    std::string cvedix_rtsp_src_node::to_string() {
        return rtsp_url;
    }
}

#endif // CVEDIX_WITH_GSTREAMER