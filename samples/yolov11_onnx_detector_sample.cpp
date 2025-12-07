#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolov11_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

/*
 * ## YOLOv11 ONNX Detector Sample ##
 * 
 * Sample code sử dụng cvedix_yolov11_detector_node để phát hiện đối tượng
 * từ video sử dụng mô hình YOLOv11 ONNX (OpenCV DNN).
 * 
 * Pipeline: Video File → YOLOv11 ONNX Detector → OSD → Screen Display
 * 
 * Yêu cầu:
 * - Model ONNX (.onnx) exported từ YOLOv11
 * - OpenCV >= 4.6 với DNN module
 * - GStreamer (cho file I/O)
 * 
 * Build:
 *   cmake -DCVEDIX_WITH_GSTREAMER=ON -DCVEDIX_BUILD_SAMPLES=ON ..
 *   make
 * 
 * Sử dụng:
 *   ./yolov11_onnx_detector_sample [model_path] [video_path] [labels_path]
 * 
 * Ví dụ:
 *   ./yolov11_onnx_detector_sample ./models/yolov11n.onnx ./test_video.mp4 ./coco_labels.txt
 */

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [model_path] [video_path] [labels_path]" << std::endl;
    std::cout << "  model_path  : Đường dẫn đến file model ONNX (.onnx)" << std::endl;
    std::cout << "  video_path  : Đường dẫn đến file video đầu vào" << std::endl;
    std::cout << "  labels_path : Đường dẫn đến file labels (tùy chọn)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " ./yolov11n.onnx ./video.mp4 ./coco_80_labels_list.txt" << std::endl;
}

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Parse command line arguments
    std::string model_path = "./cvedix_data/models/face/face_detection_yolov11_fp16.onnx";
    std::string video_path = "./cvedix_data/test_video/vehicle_count.mp4";
    std::string labels_path = "./cvedix_data/models/coco_80_labels_list.txt";
    
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
    
    if (argc > 3) {
        labels_path = argv[3];
    }

    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("YOLOv11 ONNX Detector Sample (OpenCV DNN)");
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("Model: " + model_path);
    CVEDIX_INFO("Video: " + video_path);
    CVEDIX_INFO("Labels: " + labels_path);
    CVEDIX_INFO("==================================================");

    // 1. Create File Source Node
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 
        0, 
        video_path, 
        1.0f,  // No resize
        true   // Loop video
    );
    
    // 2. Create YOLOv11 ONNX Detector Node
    auto yolov11_detector = std::make_shared<cvedix_nodes::cvedix_yolov11_detector_node>(
        "yolov11_detector",
        model_path,
        labels_path,
        640,   // Input width
        640,   // Input height
        80,    // Number of classes (COCO)
        0.25f, // Score threshold
        0.45f  // NMS threshold
    );
    
    // 3. Create OSD Node
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    
    // 4. Create Screen Output Node
    auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

    // 5. Build Pipeline
    yolov11_detector->attach_to({file_src});
    osd->attach_to({yolov11_detector});
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

