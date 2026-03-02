#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolov11_plate_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

/*
 * ## YOLOv11 License Plate Detector Sample ##
 * 
 * Sample code sử dụng cvedix_yolov11_plate_detector_node để phát hiện biển số xe
 * từ video sử dụng mô hình YOLOv11 ONNX.
 * 
 * Pipeline: Video File → YOLOv11 Plate Detector → OSD → Screen Display
 * 
 * Yêu cầu:
 * - Model ONNX (.onnx) fine-tuned cho license plate detection
 * - OpenCV >= 4.6 với DNN module
 * - GStreamer (cho file I/O)
 * 
 * Build:
 *   cmake -DCVEDIX_WITH_GSTREAMER=ON -DCVEDIX_BUILD_SAMPLES=ON ..
 *   make
 * 
 * Sử dụng:
 *   ./yolov11_plate_detector_sample [model_path] [video_path]
 * 
 * Ví dụ:
 *   ./yolov11_plate_detector_sample ./cvedix_data/models/onnx/plate/yolov11/license-plate-finetune-v1x.onnx ./cvedix_data/test_video/plate.mp4
 */

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [model_path] [video_path]" << std::endl;
    std::cout << "  model_path  : Đường dẫn đến file model ONNX (.onnx)" << std::endl;
    std::cout << "  video_path  : Đường dẫn đến file video đầu vào" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " ./cvedix_data/models/onnx/plate/yolov11/license-plate-finetune-v1x.onnx ./cvedix_data/test_video/plate.mp4" << std::endl;
}

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Parse command line arguments
    std::string model_path = "./cvedix_data/models/onnx/plate/yolov11/license-plate-finetune-v1x.onnx";
    std::string video_path = "./cvedix_data/test_video/plate.mp4";
    
    if (argc > 1) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        model_path = argv[1];
    }
    
    if (argc > 2) {
        video_path = argv[2];
    }

    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("YOLOv11 License Plate Detector Sample");
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("Model: " + model_path);
    CVEDIX_INFO("Video: " + video_path);
    CVEDIX_INFO("==================================================");

    // 1. Create File Source Node
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 
        0, 
        video_path, 
        1.0f,  // No resize
        true   // Loop video
    );
    
    // 2. Create YOLOv11 Plate Detector Node
    auto plate_detector = std::make_shared<cvedix_nodes::cvedix_yolov11_plate_detector_node>(
        "plate_detector",
        model_path,
        640,   // Input width
        640,   // Input height
        1,     // Number of classes (license plate only)
        0.25f, // Score threshold
        0.45f  // NMS threshold
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

    // 7. Display Analysis Board (if DISPLAY available)
    const char* display_env = std::getenv("DISPLAY");
    if (display_env != nullptr && strlen(display_env) > 0) {
        cvedix_utils::cvedix_analysis_board board({file_src});
        board.display(1, false);
    } else {
        CVEDIX_WARN("No DISPLAY detected, skipping analysis board.");
    }

    CVEDIX_INFO("Pipeline running. Press Enter to stop...");

    // Wait for user input
    std::string wait;
    std::getline(std::cin, wait);
    
    // Stop pipeline
    file_src->detach_recursively();
    
    CVEDIX_INFO("Pipeline stopped.");
    
    return 0;
}
