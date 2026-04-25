/**
 * @file plate_recognition_pipeline_sample.cpp
 * @brief License Plate Recognition Pipeline Sample
 * 
 * Pipeline: Video -> YOLOv11 Detector (TRT) -> PaddleOCR Recognizer -> OSD -> Screen
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
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include <iostream>

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [video] [yolo_engine] [ocr_dir]" << std::endl;
    std::cout << "  video: path to video file" << std::endl;
    std::cout << "  yolo_engine: path to YOLOv11 TRT engine" << std::endl;
    std::cout << "  ocr_dir: root dir containing PaddleOCR models (det, cls, rec, keys)" << std::endl;
}

int main(int argc, char** argv) {
    CVEDIX_LOGGER_INIT();
    
    std::string video_path = "./cvedix_data/video/plate.mp4";
    std::string engine_path = "./cvedix_data/models/tensorrt/license-plate-finetune-v1n.engine";
    std::string ocr_root = "./cvedix_data/models/paddle/ocr"; // Default root
    
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }
    
    if (argc > 1) video_path = argv[1];
    if (argc > 2) engine_path = argv[2];
    if (argc > 3) ocr_root = argv[3];
    
    // PaddleOCR paths
    // Assuming standard directory structure inside ocr_root
    std::string det_dir = ocr_root + "/ch_PP-OCRv3_det_infer";
    std::string cls_dir = ocr_root + "/ch_ppocr_mobile_v2.0_cls_infer";
    std::string rec_dir = ocr_root + "/ch_PP-OCRv3_rec_infer";
    std::string keys_path = ocr_root + "/ppocr_keys_v1.txt";
    
    CVEDIX_INFO("--- Configuration ---");
    CVEDIX_INFO("Video: " + video_path);
    CVEDIX_INFO("YOLO Engine: " + engine_path);
    CVEDIX_INFO("OCR Det: " + det_dir);
    CVEDIX_INFO("OCR Rec: " + rec_dir);
    
    try {
        // 1. Source
        auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            "src", 0, video_path, 1.0f, true
        );
        
        // 2. Detector (Primary)
        auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
            "plate_detector", engine_path, "", 0.45f, 0.45f
        );
        
        // 3. Recognizer (Secondary)
        // Apply to class ID 0 (which is "plate" in the detector)
        auto recognizer = std::make_shared<cvedix_nodes::cvedix_plate_recogniton_ppocr3>(
            "plate_recognizer",
            det_dir, cls_dir, rec_dir, keys_path,
            std::vector<int>{0}, // Apply to class 0
            0, 0, // min width/height
            2, // padding
            false, // use_tensorrt (Disabled due to CPU-only lib in test env)
            "fp16" // precision
        );
        
        // 4. OSD
        auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
        
        // 5. Display
        auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);
        
        // Link
        detector->attach_to({src});
        recognizer->attach_to({detector});
        osd->attach_to({recognizer});
        screen->attach_to({osd});
        
        // 7. Display Analysis Board
        cvedix_utils::cvedix_analysis_board board({src});
        board.display(1, false);

        CVEDIX_INFO("Starting pipeline...");
        src->start();
        
        // Wait
        std::cout << "Press Enter to stop..." << std::endl;
        std::cin.get();
        
        src->detach_recursively();
        CVEDIX_INFO("Stopped.");
        
    } catch (const std::exception& e) {
        CVEDIX_ERROR("Exception: " + std::string(e.what()));
        return -1;
    }
    
    return 0;
}

#else

#include <iostream>
int main() {
    std::cerr << "Requires CVEDIX_WITH_TRT and CVEDIX_WITH_PADDLE" << std::endl;
    return 1;
}

#endif
