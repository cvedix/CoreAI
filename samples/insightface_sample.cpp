#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_insight_face_recognition_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

/*
 * ============================================================================
 * InsightFace ONNX Recognition Sample
 * ============================================================================
 * 
 * Mô tả:
 *   Sample đơn giản để demo InsightFace face recognition với ONNX model.
 *   Trích xuất face embeddings từ ONNX model (không cần TensorRT).
 * 
 * Pipeline:
 *   Video File → YuNet Detector → InsightFace Recognition → OSD → Screen
 * 
 * Usage:
 *   ./insightface_sample [video_path] [onnx_model_path]
 * 
 * Example:
 *   ./insightface_sample \
 *     ./cvedix_data/test_video/face.mp4 \
 *     ./cvedix_data/models/face/face_recognition_sface_2021dec.onnx
 * 
 * Yêu cầu:
 *   - ONNX model: face_recognition_sface_2021dec.onnx hoặc model InsightFace khác
 *   - Test video: ./cvedix_data/test_video/face.mp4
 *   - Build với: cmake .. (không cần TensorRT)
 * 
 * Model có sẵn:
 *   - face_recognition_sface_2021dec.onnx (512-dim embeddings)
 *   - Có thể download từ: https://github.com/deepinsight/insightface
 * 
 * ============================================================================
 */

int main(int argc, char* argv[]) {
    // Configure logging
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Parse arguments
    std::string video_path = "./cvedix_data/test_video/face.mp4";
    std::string onnx_model_path = "./cvedix_data/models/face/face_recognition/w600k_mbf.onnx";
    
    if (argc >= 2) video_path = argv[1];
    if (argc >= 3) onnx_model_path = argv[2];

    std::cout << "========================================" << std::endl;
    std::cout << "InsightFace ONNX Recognition Sample" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Video: " << video_path << std::endl;
    std::cout << "  Model: " << onnx_model_path << std::endl;
    std::cout << "  Alignment: Enabled (5-point landmarks)" << std::endl;
    std::cout << "  Embedding: Auto-detected from model" << std::endl;
    std::cout << "  Backend: OpenCV DNN (ONNX Runtime)" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // ========================================
    // CREATE NODES
    // ========================================

    // Video Source
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6  // slow down for easier viewing
    );

    // Face Detector (YuNet)
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
        "detector", 
        "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
        0.9f,   // score threshold
        0.3f,   // NMS threshold
        5000    // top_k (max faces)
    );

    // Face Recognition (InsightFace ONNX) ⭐
    auto recognizer = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>(
        "recognizer",
        onnx_model_path,
        112,    // input width (standard for InsightFace)
        112,    // input height
        true    // enable_alignment = true (use 5-point landmarks)
    );

    // OSD (visualization)
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd");

    // Screen Display
    auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    // ========================================
    // CONSTRUCT PIPELINE
    // ========================================

    detector->attach_to({file_src});
    recognizer->attach_to({detector});
    osd->attach_to({recognizer});
    screen->attach_to({osd});

    std::cout << "Pipeline: file_src → detector → recognizer → osd → screen\n" << std::endl;

    // ========================================
    // START PIPELINE
    // ========================================

    std::cout << "Starting pipeline..." << std::endl;
    std::cout << "Press ENTER to stop\n" << std::endl;
    
    file_src->start();

    // Analysis board (performance monitoring)
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.display(1, false);

    // Wait for user input
    std::string wait;
    std::getline(std::cin, wait);

    // Cleanup
    std::cout << "\nStopping pipeline..." << std::endl;
    file_src->detach_recursively();
    std::cout << "Pipeline stopped." << std::endl;

    return 0;
}


