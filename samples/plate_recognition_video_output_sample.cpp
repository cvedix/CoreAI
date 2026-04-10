/**
 * @file plate_recognition_video_output_sample.cpp
 * @brief License Plate Detection + Recognition with Video Output
 * 
 * Pipeline: Video -> YOLOv11 TRT Detector -> PaddleOCR Recognizer -> OSD -> File Output
 * 
 * Requirements:
 * - cvedix compiled with -DCVEDIX_WITH_TRT=ON and -DCVEDIX_WITH_PADDLE=ON
 */

#if defined(CVEDIX_WITH_TRT) && defined(CVEDIX_WITH_PADDLE)

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/infers/cvedix_plate_recogniton_ppocr3.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include <iostream>
#include <chrono>
#include <thread>

void print_usage(const char* prog) {
    std::cout << "License Plate Recognition with Video Output" << std::endl;
    std::cout << "Usage: " << prog << " [video] [yolo_engine] [ocr_dir] [output]" << std::endl;
    std::cout << std::endl;
    std::cout << "Arguments:" << std::endl;
    std::cout << "  video:       Path to input video file" << std::endl;
    std::cout << "  yolo_engine: Path to YOLOv11 TRT engine for plate detection" << std::endl;
    std::cout << "  ocr_dir:     Root directory containing PaddleOCR models" << std::endl;
    std::cout << "  output:      Path to output video file (e.g., output.mp4)" << std::endl;
}

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();
    
    // Default paths
    std::string video_path = "./cvedix_data/test_video/vietnam_plate.mp4";
    std::string engine_path = "./cvedix_data/models/tensorrt/license-plate-finetune-v1x-trt10.engine";
    std::string ocr_root = "./cvedix_data/models/text/ppocr";
    std::string output_path = "./output/vietnam_plate_recognized.mp4";
    
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (argc > 1) video_path = argv[1];
    if (argc > 2) engine_path = argv[2];
    if (argc > 3) ocr_root = argv[3];
    if (argc > 4) output_path = argv[4];
    
    // PaddleOCR paths
    std::string det_dir = ocr_root + "/ch_PP-OCRv3_det_infer";
    std::string cls_dir = ocr_root + "/ch_ppocr_mobile_v2.0_cls_infer";
    std::string rec_dir = ocr_root + "/ch_PP-OCRv3_rec_infer";
    std::string keys_path = ocr_root + "/ppocr_keys_v1.txt";
    
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("License Plate Recognition with Video Output");
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("Input:  " + video_path);
    CVEDIX_INFO("Engine: " + engine_path);
    CVEDIX_INFO("OCR:    " + ocr_root);
    CVEDIX_INFO("Output: " + output_path);
    CVEDIX_INFO("==================================================");
    
    try {
        // 1. Source - no loop, process full video
        auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            "src", 0, video_path, 1.0f, false  // no loop
        );
        
        // 2. Detector (Primary) - YOLOv11 TensorRT
        auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
            "plate_detector", engine_path, "", 0.35f, 0.45f
        );
        
        // 3. Recognizer (Secondary) - PaddleOCR
        // Apply to class ID 0 (which is "plate" in the detector)
        auto recognizer = std::make_shared<cvedix_nodes::cvedix_plate_recogniton_ppocr3>(
            "plate_recognizer",
            det_dir, cls_dir, rec_dir, keys_path,
            std::vector<int>{0}, // Apply to class 0 (license plate)
            0, 0,   // min width/height
            2,      // padding
            false,  // use_tensorrt (use CPU for OCR)
            "fp32"  // precision
        );
        
        // 4. OSD - draw plate boxes and recognized text (using standard OSD for plain text labels)
        auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
        
        // 5. File Output - takes (name, channel, save_dir, prefix, duration)
        auto file_out = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
            "file_out", 0, 
            "./video_output",   // save directory (must exist)
            "vietnam_plate_",   // filename prefix
            5,                  // 5 minutes max per file
            cvedix_objects::cvedix_size{0, 0}, // auto resolution
            2048,               // bitrate
            true                // OSD enabled
        );
        
        // 6. Screen output for debugging
        auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);
        
        // Link pipeline
        detector->attach_to({src});
        recognizer->attach_to({detector});
        osd->attach_to({recognizer});
        file_out->attach_to({osd});
        screen->attach_to({osd});  // Also display on screen for debugging
        
        CVEDIX_INFO("Pipeline built. Starting processing...");
        
        // Start pipeline
        src->start();
        
        CVEDIX_INFO("Processing video... Press Enter to stop or wait for completion.");
        
        // Wait for user input to stop
        std::cin.get();
        
        src->detach_recursively();
        
        CVEDIX_INFO("==================================================");
        CVEDIX_INFO("Processing complete!");
        CVEDIX_INFO("Output saved to: " + output_path);
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
    std::cerr << "This sample requires CVEDIX_WITH_TRT and CVEDIX_WITH_PADDLE" << std::endl;
    std::cerr << "Please rebuild with:" << std::endl;
    std::cerr << "  cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_PADDLE=ON .." << std::endl;
    return 1;
}

#endif
