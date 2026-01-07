/**
 * @file yolov11_plate_detector_trt_sample.cpp
 * @brief TensorRT YOLOv11 License Plate Detection Sample
 * 
 * Demonstrates high-performance license plate detection using TensorRT engine.
 * 
 * Pipeline: Video File → TensorRT Plate Detector → OSD → Screen Display
 * 
 * Requirements:
 * - TensorRT engine file (.engine) for YOLOv11 plate detection
 * - Built with -DCVEDIX_WITH_TRT=ON
 * - NVIDIA GPU with CUDA support
 * 
 * Usage:
 *   ./yolov11_plate_detector_trt_sample [engine_path] [video_path]
 * 
 * Example:
 *   ./yolov11_plate_detector_trt_sample \
 *       ./cvedix_data/models/tensorrt/license-plate-finetune-v1n.engine \
 *       ./cvedix_data/test_video/plate.mp4
 * 
 * Performance (RTX 3060 Ti):
 *   - v1n.engine: ~900 FPS
 *   - v1s.engine: ~600 FPS
 *   - v1m.engine: ~300 FPS
 */

#ifdef CVEDIX_WITH_TRT

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_plate_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [engine_path] [video_path]" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  engine_path : Path to TensorRT engine file (.engine)" << std::endl;
    std::cout << "  video_path  : Path to input video file" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " \\" << std::endl;
    std::cout << "      ./cvedix_data/models/tensorrt/license-plate-finetune-v1n.engine \\" << std::endl;
    std::cout << "      ./cvedix_data/test_video/plate.mp4" << std::endl;
    std::cout << std::endl;
    std::cout << "Available engines:" << std::endl;
    std::cout << "  license-plate-finetune-v1n.engine (fastest, ~900 FPS)" << std::endl;
    std::cout << "  license-plate-finetune-v1s.engine (balanced, ~600 FPS)" << std::endl;
    std::cout << "  license-plate-finetune-v1m.engine (accurate, ~300 FPS)" << std::endl;
}

int main(int argc, char** argv) {
    // Configure logging
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Parse command line arguments
    std::string engine_path = "./cvedix_data/models/tensorrt/license-plate-finetune-v1n.engine";
    std::string video_path = "./cvedix_data/test_video/plate.mp4";
    float conf_threshold = 0.25f;
    float nms_threshold = 0.45f;
    
    if (argc > 1) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        engine_path = argv[1];
    }
    
    if (argc > 2) {
        video_path = argv[2];
    }
    
    if (argc > 3) {
        conf_threshold = std::stof(argv[3]);
    }
    
    if (argc > 4) {
        nms_threshold = std::stof(argv[4]);
    }

    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("TensorRT YOLOv11 License Plate Detector Sample");
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("Engine: " + engine_path);
    CVEDIX_INFO("Video:  " + video_path);
    CVEDIX_INFO("Conf:   " + std::to_string(conf_threshold));
    CVEDIX_INFO("NMS:    " + std::to_string(nms_threshold));
    CVEDIX_INFO("==================================================");

    try {
        // 1. Create File Source Node
        auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            "file_src", 
            0,              // channel index
            video_path, 
            1.0f,           // no resize
            true            // loop video
        );
        
        // 2. Create TensorRT Plate Detector Node
        auto plate_detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_plate_detector_node>(
            "plate_detector",
            engine_path,
            conf_threshold,
            nms_threshold
        );
        
        // 3. Create OSD Node to draw bounding boxes
        auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
        
        // 4. Create Screen Output Node
        auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

        // 5. Build Pipeline
        plate_detector->attach_to({file_src});
        osd->attach_to({plate_detector});
        screen_des->attach_to({osd});

        CVEDIX_INFO("Pipeline built. Starting processing...");
        
        // 6. Start Pipeline
        file_src->start();

        // 7. Display Analysis Board (Removed by request)
        cvedix_utils::cvedix_analysis_board board({file_src});
        board.display(1, false);

        CVEDIX_INFO("Pipeline running. Press Enter to stop...");

        // Wait for user input
        std::string wait;
        std::getline(std::cin, wait);
        
        // Stop pipeline
        file_src->detach_recursively();
        
        CVEDIX_INFO("Pipeline stopped.");
    }
    catch (const std::exception& e) {
        CVEDIX_ERROR("Error: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}

#else

#include <iostream>

int main() {
    std::cerr << "This sample requires TensorRT support." << std::endl;
    std::cerr << "Please rebuild with -DCVEDIX_WITH_TRT=ON" << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_TRT
