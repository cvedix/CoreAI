#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"

#ifdef CVEDIX_WITH_LICENSE
#include "cvedix/utils/license/cvedix_license_manager.h"
#endif

// trt_vehicle is currently disabled due to TensorRT 10.x API incompatibility
// #ifdef CVEDIX_WITH_TRT
// #include "cvedix/nodes/infers/cvedix_trt_vehicle_detector.h"
// #endif

#ifdef CVEDIX_WITH_RKNN
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#endif

#ifdef CVEDIX_WITH_TRT
#include "cvedix/nodes/infers/cvedix_insight_face_recognition_node.h"
#endif

#include <iostream>
#include <memory>

/**
 * @brief Sample demonstrating license checking for protected features
 * 
 * This sample shows:
 * 1. How to check license status using licensecxx
 * 2. How to handle license errors when creating protected nodes
 * 3. How non-protected features work without license
 * 
 * Note: Requires licensecxx library and valid license file (JSON format)
 *       with corresponding public key for verification.
 */
int main() {
    // Initialize logger
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_SET_LOG_DIR("./log");
    CVEDIX_LOGGER_INIT();
    
    std::cout << "================================================" << std::endl;
    std::cout << "CVEDIX License Check Sample" << std::endl;
    std::cout << "================================================" << std::endl;
    
    #ifdef CVEDIX_WITH_LICENSE
    // Check license status
    std::cout << "\n[1] Checking license status..." << std::endl;
    auto& license_mgr = cvedix_utils::cvedix_license_manager::get_instance();
    
    std::string license_path = license_mgr.get_license_path();
    std::cout << "License file path: " << license_path << std::endl;
    
    bool is_licensed = license_mgr.check_license();
    if (is_licensed) {
        std::cout << "✓ License is valid" << std::endl;
    } else {
        std::cout << "✗ License is invalid or not found" << std::endl;
        std::cout << "  Protected features (TensorRT, RKNN, InsightFace) will not work" << std::endl;
    }
    
    // Try to create protected nodes
    std::cout << "\n[2] Testing protected features..." << std::endl;
    
    #ifdef CVEDIX_WITH_TRT
    std::cout << "\n[2.1] Testing TensorRT node..." << std::endl;
    // trt_vehicle is currently disabled due to TensorRT 10.x API incompatibility
    // Using trt_insightface as an example instead
    std::cout << "  Note: trt_vehicle nodes are temporarily disabled due to TensorRT 10.x API incompatibility" << std::endl;
    std::cout << "  TensorRT is available (trt_insightface is enabled)" << std::endl;
    #else
    std::cout << "[2.1] TensorRT not enabled (CVEDIX_WITH_TRT=OFF)" << std::endl;
    #endif
    
    #ifdef CVEDIX_WITH_RKNN
    std::cout << "\n[2.2] Testing RKNN node..." << std::endl;
    try {
        auto rknn_detector = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
            "test_rknn_detector",
            "./models/yolov8n.rknn",  // This path may not exist, but license check happens first
            0.5f, 0.5f, 640, 640, 80
        );
        std::cout << "✓ RKNN node created successfully" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "✗ Failed to create RKNN node: " << e.what() << std::endl;
    }
    #else
    std::cout << "[2.2] RKNN not enabled (CVEDIX_WITH_RKNN=OFF)" << std::endl;
    #endif
    
    #ifdef CVEDIX_WITH_TRT
    std::cout << "\n[2.3] Testing InsightFace node..." << std::endl;
    try {
        auto insightface_node = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>(
            "test_insightface",
            "./models/arcface_r100.onnx",  // This path may not exist, but license check happens first
            112, 112, true
        );
        std::cout << "✓ InsightFace node created successfully" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "✗ Failed to create InsightFace node: " << e.what() << std::endl;
    }
    #else
    std::cout << "[2.3] InsightFace not enabled (CVEDIX_WITH_TRT=OFF)" << std::endl;
    #endif
    
    #else
    std::cout << "\n[1] License checking is disabled (CVEDIX_WITH_LICENSE=OFF)" << std::endl;
    std::cout << "All features work without license check (development mode)" << std::endl;
    
    std::cout << "\n[2] Testing features..." << std::endl;
    
    #ifdef CVEDIX_WITH_TRT
    std::cout << "\n[2.1] TensorRT is available (no license check)" << std::endl;
    #else
    std::cout << "[2.1] TensorRT not enabled" << std::endl;
    #endif
    
    #ifdef CVEDIX_WITH_RKNN
    std::cout << "[2.2] RKNN is available (no license check)" << std::endl;
    #else
    std::cout << "[2.2] RKNN not enabled" << std::endl;
    #endif
    
    std::cout << "[2.3] InsightFace is available (no license check)" << std::endl;
    #endif
    
    std::cout << "\n================================================" << std::endl;
    std::cout << "Sample completed" << std::endl;
    std::cout << "================================================" << std::endl;
    
    return 0;
}

