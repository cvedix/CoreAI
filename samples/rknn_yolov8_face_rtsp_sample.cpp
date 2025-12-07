#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

/*
* ## RKNN YOLOv8 Face Detection from RTSP Sample ##
* 
* Face detection from RTSP camera stream using YOLOv8 RKNN model.
* 
* Pipeline:
*   RTSP Source -> YOLOv8 Face Detector (RKNN) -> OSD -> Screen Display
* 
* Model: yolov8n_face_detection.rknn
*   - YOLOv8 nano model trained for face detection
*   - Optimized for Rockchip NPU (RK3588/RK3568/RK3566)
*   - Input: 640x640
*   - Output: Bounding boxes around faces
* 
* Build:
*   cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_BUILD_SAMPLES=ON ..
*   make
* 
* Usage:
*   ./rknn_yolov8_face_rtsp_sample [rtsp_url] [model_path]
* 
* Example:
*   ./rknn_yolov8_face_rtsp_sample
*   ./rknn_yolov8_face_rtsp_sample rtsp://user:pass@192.168.1.100:554/stream
*   ./rknn_yolov8_face_rtsp_sample rtsp://camera-url ./yolov8n_face.rknn
*/

volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Configuration
    std::string rtsp_url = "rtsp://cvedix:Admin123456@192.168.1.209:554/stream1";
    std::string model_path = "./cvedix_data/models/face/yolov8n_face_detection.rknn";

    if (argc > 1) {
        rtsp_url = argv[1];
    }
    if (argc > 2) {
        model_path = argv[2];
    }

    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("RKNN YOLOv8 Face Detection from RTSP");
    CVEDIX_INFO("==================================================");
    CVEDIX_INFO("RTSP URL: " + rtsp_url);
    CVEDIX_INFO("Model:    " + model_path);
    CVEDIX_INFO("==================================================");

    // 1. Create RTSP Source
    auto rtsp_src = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "rtsp_src",
        0,
        rtsp_url,
        1.0f,       // No resize
        "mppvideodec",  // MPP hardware decoder for Rockchip
        0,          // No frame skip
        "auto"      // Auto-detect H264/H265
    );

    // 2. Create RKNN YOLOv8 Face Detector
    auto face_detector = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
        "yolov8_face_detector",
        model_path,
        0.5f,  // Score threshold
        0.5f,  // NMS threshold
        640,   // Input width
        640,   // Input height
        1      // 1 class (face)
    );

    // 3. Create OSD Node (draw bounding boxes)
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

    // 4. Create Screen Output
    auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

    // 5. Build Pipeline
    // rtsp_src -> face_detector -> osd -> screen_des
    face_detector->attach_to({rtsp_src});
    osd->attach_to({face_detector});
    screen_des->attach_to({osd});

    CVEDIX_INFO("Starting pipeline...");

    // 6. Start Pipeline
    rtsp_src->start();

    CVEDIX_INFO("Pipeline started. Press Ctrl+C to stop...");

    // Keep running until Ctrl+C
    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    CVEDIX_INFO("Stopping pipeline...");
    rtsp_src->detach_recursively();

    return 0;
}

