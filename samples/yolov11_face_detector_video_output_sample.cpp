/**
 * @file yolov11_face_detector_video_output_sample.cpp
 * @brief YOLOv11 Face Detection with Video Output
 * 
 * Pipeline: Video -> YOLOv11 TRT Face Detector -> OSD -> File Output + Screen
 * 
 * Requirements:
 * - cvedix compiled with -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON
 * - TensorRT engine file for YOLOv11 face detection
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    g_running = false;
}

void print_usage(const char* prog) {
    std::cout << "YOLOv11 Face Detection with Video Output" << std::endl;
    std::cout << "Usage: " << prog << " [engine] [video] [output_dir] [conf] [nms]" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  engine:     Path to YOLOv11 Face TRT engine" << std::endl;
    std::cout << "  video:      Path to input video file" << std::endl;
    std::cout << "  output_dir: Output directory for video file" << std::endl;
    std::cout << "  conf:       Confidence threshold (default: 0.5)" << std::endl;
    std::cout << "  nms:        NMS threshold (default: 0.45)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << prog << " \\" << std::endl;
    std::cout << "      ./cvedix_data/models/trt/face/yolov11_face_fp16.engine \\" << std::endl;
    std::cout << "      ./cvedix_data/test_video/face.mp4 \\" << std::endl;
    std::cout << "      ./video_output" << std::endl;
}

int main(int argc, char** argv) {
    // Setup signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();
    
    // Default paths
    std::string engine_path = "./cvedix_data/models/trt/face/yolov11_face_fp16.engine";
    std::string video_path = "./cvedix_data/test_video/face.mp4";
    std::string output_dir = "./video_output";
    float conf_threshold = 0.5f;
    float nms_threshold = 0.45f;
    
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (argc > 1) engine_path = argv[1];
    if (argc > 2) video_path = argv[2];
    if (argc > 3) output_dir = argv[3];
    if (argc > 4) conf_threshold = std::stof(argv[4]);
    if (argc > 5) nms_threshold = std::stof(argv[5]);
    
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("YOLOv11 Face Detection with Video Output");
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("Engine:  " + engine_path);
    CVEDIX_INFO("Input:   " + video_path);
    CVEDIX_INFO("Output:  " + output_dir);
    CVEDIX_INFO("Conf:    " + std::to_string(conf_threshold));
    CVEDIX_INFO("NMS:     " + std::to_string(nms_threshold));
    CVEDIX_INFO("==================================================");
    
    try {
        // 1. Source - no loop, process full video once
        auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            "src", 0, video_path, 1.0f, false  // no loop
        );
        
        // 2. Face Detector - YOLOv11 TensorRT
        auto face_detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_face_detector_node>(
            "face_detector", engine_path, conf_threshold, nms_threshold
        );
        
        // 3. OSD - draw face boxes (use cvedix_face_osd_node for face_targets)
        auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
        
        // 4. File Output - save to video file
        auto file_out = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
            "file_out", 0, 
            output_dir,                  // save directory
            "face_detection_",           // filename prefix
            30,                          // 30 minutes max per file (max allowed)
            cvedix_objects::cvedix_size{0, 0}, // auto resolution
            4096,                        // bitrate
            true                         // OSD enabled
        );
        
        // 5. Screen output for debugging (optional - comment out for headless)
        auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);
        
        // Link pipeline
        face_detector->attach_to({src});
        osd->attach_to({face_detector});
        file_out->attach_to({osd});
        screen->attach_to({osd});  // Also display on screen
        
        CVEDIX_INFO("Pipeline built successfully:");
        CVEDIX_INFO("  src -> face_detector -> osd -> file_out + screen");
        CVEDIX_INFO("Starting processing...");
        
        // Track face count for statistics
        int total_faces = 0;
        int frame_count = 0;
        
        // Hook to count detections
        auto count_hook = [&total_faces, &frame_count](
            std::string node_name, int queue_size,
            std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
            auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
            if (fm) {
                total_faces += fm->face_targets.size();
                frame_count++;
                
                if (frame_count % 100 == 0) {
                    std::cout << "\r[Progress] Frames: " << frame_count 
                              << ", Total faces detected: " << total_faces << std::flush;
                }
            }
        };
        face_detector->set_meta_handled_hooker(count_hook);
        
        // Start pipeline
        src->start();
        
        CVEDIX_INFO("Processing video for 15 seconds...");
        
        // Process for fixed duration (15 seconds)
        auto start_time = std::chrono::steady_clock::now();
        while (g_running) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= 15) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << std::endl;  // New line after progress
        
        // Cleanup
        src->detach_recursively();
        
        // Wait a bit for file to be written
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        CVEDIX_INFO("==================================================");
        CVEDIX_INFO("Processing complete!");
        CVEDIX_INFO("Total frames processed: " + std::to_string(frame_count));
        CVEDIX_INFO("Total faces detected: " + std::to_string(total_faces));
        CVEDIX_INFO("Average faces per frame: " + std::to_string(frame_count > 0 ? (float)total_faces/frame_count : 0));
        CVEDIX_INFO("Output saved to: " + output_dir);
        CVEDIX_INFO("==================================================");
        
    } catch (const std::exception& e) {
        CVEDIX_ERROR("Exception: " + std::string(e.what()));
        return -1;
    }
    
    return 0;
}

#else

#include <iostream>

int main() {
    std::cerr << "This sample requires CVEDIX_WITH_TRT" << std::endl;
    std::cerr << "Please rebuild with:" << std::endl;
    std::cerr << "  cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON .." << std::endl;
    return 1;
}

#endif
