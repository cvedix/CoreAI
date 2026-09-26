/**
 * @file heraface_lite_sample.cpp
 * @brief HeraFace Lite proof-of-concept for a small-scale embedded face SDK.
 *
 * This sample is intentionally aligned with the real CoreAI build path, not the
 * incomplete standalone SDK package. It uses the actual exported face nodes that
 * are built into the project and supports a local database for roughly 50 users.
 */

#ifndef CVEDIX_WITH_FACE
#error "This sample requires CVEDIX_WITH_FACE=ON. Rebuild the project with the SeetaFace6 option enabled."
#endif

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_recognizer_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::string video_path = argc > 1 ? argv[1] : "data/videos/16.22.09.mp4";
    std::string model_dir = argc > 2 ? argv[2] : "./cvedix_data/models/seetaface6";
    std::string db_path = argc > 3 ? argv[3] : "./cvedix_data/face_db/heraface_lite";

    if (!std::filesystem::exists(video_path)) {
        std::cerr << "Video file not found: " << video_path << "\n";
        std::cerr << "Usage: ./heraface_lite_sample [video_path] [seetaface_model_dir] [db_path]\n";
        return 1;
    }

    if (!std::filesystem::exists(model_dir)) {
        std::cerr << "SeetaFace6 model dir not found: " << model_dir << "\n";
        std::cerr << "Expected files include face_detector.csta and face_recognizer.csta.\n";
        return 1;
    }

    std::cout << "=== HeraFace Lite prototype ===\n";
    std::cout << "Video: " << video_path << "\n";
    std::cout << "SeetaFace6 model dir: " << model_dir << "\n";
    std::cout << "Local face DB: " << db_path << "\n";

    auto source = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "source",
        0,
        video_path,
        1.0f,
        true,
        "avdec_h264",
        0,
        true
    );

    auto recognizer = std::make_shared<cvedix_nodes::cvedix_face_recognizer_node>(
        "face_recognizer",
        model_dir,
        db_path,
        0.7f,
        40,
        false,
        false,
        0,
        cvedix_nodes::FaceRecognizerMode::SYNC
    );

    auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>(
        "screen",
        0,
        true,
        cvedix_objects::cvedix_size{}
    );

    recognizer->attach_to({source});
    screen->attach_to({recognizer});

    source->start();

    std::cout << "\nHeraFace Lite pipeline running. Press Enter to stop.\n";
    std::string wait;
    std::getline(std::cin, wait);

    source->detach_recursively();
    return 0;
}
