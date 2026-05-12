/**
 * @file face_analysis_seetaface_sample.cpp
 * @brief SeetaFace6 Full Face Analysis + Web Debug Dashboard
 *
 * Pipeline:
 *   file_src (loop) → face_analysis_node → osd → web_debug_des
 *
 * Features:
 *   - Face detection with SeetaFace6
 *   - Age & gender prediction
 *   - Eye state detection (open/closed)
 *   - Head pose estimation (yaw/pitch/roll)
 *   - Anti-spoofing / liveness detection
 *   - Face recognition (optional, via database)
 *   - Mask detection
 *   - OSD overlay with all attributes
 *   - Web-based debug dashboard with Analysis Board
 *
 * Usage:
 *   ./face_analysis_seetaface_sample [video] [model_dir] [port] [face_db]
 *
 * Open http://localhost:9092 in browser to see live dashboard.
 */

#ifdef CVEDIX_WITH_FACE

#include "cvedix/nodes/infers/cvedix_face_analysis_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string first_existing(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) return path;
    }
    return candidates.empty() ? "" : candidates.front();
}

void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [video_path] [model_dir] [port] [face_db_path]\n"
        << "  video_path  : input video path\n"
        << "  model_dir   : SeetaFace6 model directory (containing .csta files)\n"
        << "  port        : web dashboard port, default 9092\n"
        << "  face_db_path: face database path for recognition (optional)\n\n"
        << "Pipeline:\n"
        << "  file_src → face_analysis → osd → web_debug\n\n"
        << "Features:\n"
        << "  - Age & Gender prediction\n"
        << "  - Eye state detection\n"
        << "  - Head pose estimation (Yaw/Pitch/Roll)\n"
        << "  - Anti-spoofing / Liveness detection\n"
        << "  - Face recognition (if database loaded)\n"
        << "  - Mask detection\n\n"
        << "Example:\n"
        << "  " << program
        << " ./cvedix_data/videos/face.mp4"
        << " ./cvedix_data/models/seetaface6/sf3.0_models 9092\n";
}

}  // namespace

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        print_usage(argv[0]);
        return 0;
    }

    // ── Auto-detect paths ──
    std::string video_path = first_existing({
        "../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "../cvedix_data/videos/face.mp4",
        "./cvedix_data/videos/face.mp4"
    });

    std::string model_dir = first_existing({
        "../cvedix_data/models/seetaface6/sf3.0_models",
        "./cvedix_data/models/seetaface6/sf3.0_models",
        "../third_party/seetaface6/build/include/seeta"
    });

    int port = 9092;
    std::string db_path = "";

    if (argc > 1) video_path = argv[1];
    if (argc > 2) model_dir = argv[2];
    if (argc > 3) port = std::stoi(argv[3]);
    if (argc > 4) db_path = argv[4];

    if (!std::filesystem::exists(video_path)) {
        std::cerr << "Video not found: " << video_path << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    if (!std::filesystem::exists(model_dir)) {
        std::cerr << "SeetaFace6 model directory not found: " << model_dir << std::endl;
        std::cerr << "Expected .csta model files in the directory.\n";
        print_usage(argv[0]);
        return 1;
    }

    CVEDIX_INFO("╔══════════════════════════════════════════════════════════════╗");
    CVEDIX_INFO("║  SeetaFace6 Full Face Analysis + Web Debug Dashboard       ║");
    CVEDIX_INFO("╚══════════════════════════════════════════════════════════════╝");
    CVEDIX_INFO("Video:     " + video_path);
    CVEDIX_INFO("Models:    " + model_dir);
    CVEDIX_INFO("Port:      " + std::to_string(port));
    CVEDIX_INFO("Face DB:   " + (db_path.empty() ? "(none)" : db_path));

    // ══════════════════════════════════════
    // 1. Create pipeline nodes
    // ══════════════════════════════════════

    // Source: file with loop
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6f, true);

    // Face analysis: all SeetaFace6 modules
    auto analyzer = std::make_shared<cvedix_nodes::cvedix_face_analysis_node>(
        "face_analyzer",
        model_dir,
        db_path,          // face database (empty = no recognition)
        0.70f,            // similarity threshold
        80,               // min face size
        false,            // anti-spoofing OFF (heavy, ~200ms per face)
        true,             // enable age/gender
        true,             // enable eye state
        true              // enable pose estimation
    );
    analyzer->setFrameSkip(3);   // Analyze every 3rd frame for ~3x FPS boost

    // OSD: rendering config
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_config;
    osd_config.show_bbox = true;
    osd_config.show_label = true;
    osd_config.show_track_id = false;
    osd_config.show_track_trail = false;
    osd_config.show_center_dot = false;
    osd_config.show_static_lines = false;
    osd_config.show_static_zones = false;
    osd_config.enable_ba_crossline = false;
    osd_config.enable_ba_crowding = false;
    osd_config.enable_ba_jam = false;
    osd_config.enable_ba_stop = false;
    osd_config.enable_ba_enter_exit = false;
    osd_config.enable_face = true;
    osd_config.enable_face_blur = false;
    osd_config.bbox_color = cv::Scalar(241, 102, 99);
    osd_config.label_color = cv::Scalar(250, 139, 167);
    osd_config.label_font_scale = 0.45;
    osd_config.bbox_thickness = 2;
    osd->update_config(osd_config);

    // Web debug destination
    auto web_debug = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, port, nullptr, 50);

    // ══════════════════════════════════════
    // 2. Build pipeline
    // ══════════════════════════════════════
    analyzer->attach_to({file_src});
    osd->attach_to({analyzer});
    web_debug->attach_to({osd});

    // ══════════════════════════════════════
    // 3. Analysis Board
    // ══════════════════════════════════════
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);
    web_debug->set_board(&board);

    // ══════════════════════════════════════
    // 4. Start pipeline
    // ══════════════════════════════════════
    file_src->start();

    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║  SeetaFace6 Full Face Analysis Dashboard                   ║\n"
              << "║                                                            ║\n"
              << "║  Pipeline:                                                 ║\n"
              << "║    file_src → face_analysis → osd → web_debug             ║\n"
              << "║                                                            ║\n"
              << "║  Features per face:                                        ║\n"
              << "║    • Age prediction        • Gender prediction             ║\n"
              << "║    • Eye state (L/R)       • Head pose (Y/P/R)             ║\n"
              << "║    • Anti-spoofing         • Mask detection                ║\n"
              << "║    • Face recognition      • 5-point landmarks             ║\n"
              << "║                                                            ║\n"
              << "║  Open in browser:                                          ║\n"
              << "║  → http://localhost:" << port << "                                    ║\n"
              << "║                                                            ║\n"
              << "║  Press Enter to stop...                                    ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << std::endl;

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    return 0;
}

#else

#include <iostream>

int main() {
    std::cerr << "This sample requires SeetaFace6. Build with -DCVEDIX_WITH_FACE=ON\n"
              << "First build SeetaFace6: cd third_party/seetaface6 && bash build_seetaface6.sh\n";
    return 1;
}

#endif // CVEDIX_WITH_FACE
