/**
 * @file llm_sample.cpp
 * @brief Sample: Local LLM analysis of video detections using LLM
 * 
 * This sample demonstrates the LLM pipeline node which runs
 * LLM inference locally on GPU using llama.cpp.
 * 
 * Pipeline:
 *   FileSource → YOLO Detector → LLM Analyzer → OSD → Screen
 * 
 * The LLM node receives detection results from the YOLO detector,
 * formats them into a prompt, and generates a text analysis using a
 * locally loaded GGUF model (e.g., DeepSeek Coder V2 Lite 16B Q8_0).
 * 
 * Prerequisites:
 * - Build with: cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_LLM=ON -DCVEDIX_BUILD_SAMPLES=ON ..
 * - Download a GGUF model file
 * - Prepare a video file and YOLO model
 * 
 * Usage:
 *   ./llm_sample <video_path> <yolo_model_path> <gguf_model_path>
 * 
 * Example:
 *   ./llm_sample video.mp4 yolov11n.onnx deepseek-coder-v2-lite-16b-q8_0.gguf
 */

#ifdef CVEDIX_WITH_LLM

#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/infers/cvedix_yolo_detector_node.h>
#include <cvedix/nodes/infers/cvedix_llm_node.h>
#include <cvedix/nodes/osd/cvedix_osd_node.h>
#include <cvedix/nodes/des/cvedix_screen_des_node.h>
#include <cvedix/utils/analysis_board/cvedix_analysis_board.h>

int main(int argc, char* argv[]) {
    // Initialize logger
    CVEDIX_LOGGER_INIT();

    // Parse arguments
    std::string video_path = "video.mp4";
    std::string yolo_model = "yolov11n.onnx";
    std::string gguf_model = "model.gguf";

    if (argc >= 4) {
        video_path = argv[1];
        yolo_model = argv[2];
        gguf_model = argv[3];
    } else if (argc >= 2) {
        video_path = argv[1];
    }

    CVEDIX_INFO("=== LLM Local Inference Sample ===");
    CVEDIX_INFO("Video: " + video_path);
    CVEDIX_INFO("YOLO model: " + yolo_model);
    CVEDIX_INFO("GGUF model: " + gguf_model);

    // 1. Source node — read video file
    auto source = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "src", 0, video_path, 1.0);

    // 2. YOLO detector — detect objects in each frame
    cvedix_nodes::BackendType backend = cvedix_nodes::BackendType::AUTO;
    if (yolo_model.find(".onnx") != std::string::npos) {
        backend = cvedix_nodes::BackendType::ONNX;
    } else if (yolo_model.find(".engine") != std::string::npos) {
        backend = cvedix_nodes::BackendType::TENSORRT;
    }

    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "det",
        yolo_model,                         // model path
        cvedix_nodes::YoloVersion::YOLO11,  // YOLO plugin family
        "",                                 // labels path
        0.5f,                               // confidence threshold
        0.45f,                              // NMS threshold
        0,                                  // class_id_offset
        backend
    );

    // 3. LLM node — analyze detections using local LLM
    auto llm = std::make_shared<cvedix_nodes::cvedix_llm_node>(
        "llm",
        gguf_model,                     // GGUF model path
        "You are analyzing a surveillance camera feed.\n"
        "Frame #{frame_index} contains {num_targets} detected objects:\n"
        "{detections}\n"
        "Provide a brief (2-3 sentence) summary of the scene. "
        "Focus on any safety-relevant observations.",
        -1,                             // n_gpu_layers (-1 = all on GPU)
        4096,                           // context window
        128,                            // max tokens (short responses for speed)
        0.2f,                           // temperature (low for factual)
        true                            // skip_on_busy
    );

    // 4. OSD node (disabled for headless)
    // auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

    // 5. Screen output (disabled for headless)
    // auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    // Build pipeline
    detector->attach_to({source});
    llm->attach_to({detector});
    // osd->attach_to({llm});
    // screen->attach_to({osd});

    // Start pipeline
    CVEDIX_INFO("Starting pipeline...");
    source->start();

    // Display analysis board (FPS, latency, queue stats)
    // cvedix_utils::cvedix_analysis_board board({source});
    // board.display();
    // wait for video to finish
    std::this_thread::sleep_for(std::chrono::seconds(15));

    CVEDIX_INFO("Pipeline finished.");
    source->detach_recursively();
    return 0;
}

#else

#include <iostream>

int main() {
    std::cerr << "ERROR: This sample requires CVEDIX_WITH_LLM=ON and CVEDIX_WITH_CUDA=ON." << std::endl;
    std::cerr << "Rebuild with: cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_LLM=ON -DCVEDIX_BUILD_SAMPLES=ON .." << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_LLM
