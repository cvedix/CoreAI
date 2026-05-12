/**
 * @file milvus_face_search_sample.cpp
 * @brief Large-scale Face Search with Milvus Vector Database
 *
 * Pipeline:
 *   file_src (loop) → face_analysis_node → milvus_face_search → osd → web_debug_des
 *
 * Features:
 *   - Face detection + feature extraction via SeetaFace6
 *   - Large-scale face search via Milvus vector database
 *   - Real-time OSD overlay with identification results
 *   - Web-based debug dashboard
 *   - Face registration from image directory
 *
 * Prerequisites:
 *   - Milvus server running (docker compose -f configs/docker-compose.milvus.yml up -d)
 *   - SeetaFace6 models
 *   - Build with: cmake -DCVEDIX_WITH_FACE=ON -DCVEDIX_WITH_MILVUS=ON ..
 *
 * Usage:
 *   ./milvus_face_search_sample [video] [model_dir] [milvus_uri] [register_dir] [port]
 *
 * Example:
 *   # 1. Start Milvus
 *   docker compose -f configs/docker-compose.milvus.yml up -d
 *
 *   # 2. Run with face registration
 *   ./milvus_face_search_sample \
 *       ./cvedix_data/videos/face.mp4 \
 *       ./cvedix_data/models/seetaface6/sf3.0_models \
 *       localhost:19530 \
 *       ./cvedix_data/faces/ \
 *       9093
 *
 *   # Face images should be named: PersonName.jpg, PersonName_2.jpg, etc.
 *   # Open http://localhost:9093 in browser for live dashboard.
 */

#if defined(CVEDIX_WITH_FACE) && defined(CVEDIX_WITH_MILVUS)

#include "cvedix/nodes/infers/cvedix_face_analysis_node.h"
#include "cvedix/nodes/infers/cvedix_milvus_face_search_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/imgcodecs.hpp>

// SeetaFace6 for feature extraction during registration
#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>

namespace {

std::string first_existing(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) return path;
    }
    return candidates.empty() ? "" : candidates.front();
}

/**
 * @brief Extract face name from filename
 * 
 * Examples:
 *   "JohnDoe.jpg"       → "JohnDoe"
 *   "JohnDoe_2.jpg"     → "JohnDoe"
 *   "Jane Smith.png"    → "Jane Smith"
 */
std::string nameFromFilename(const std::string& filename) {
    auto name = std::filesystem::path(filename).stem().string();
    // Remove trailing _N suffix (e.g., _2, _3)
    auto pos = name.rfind('_');
    if (pos != std::string::npos) {
        bool all_digits = true;
        for (size_t i = pos + 1; i < name.size(); i++) {
            if (!std::isdigit(name[i])) { all_digits = false; break; }
        }
        if (all_digits && pos > 0) {
            name = name.substr(0, pos);
        }
    }
    return name;
}

/**
 * @brief Register faces from a directory into Milvus
 * 
 * Reads all .jpg/.png images, extracts face embeddings using SeetaFace6,
 * and registers them in the Milvus face search node.
 */
int registerFacesFromDir(
    const std::string& dir_path,
    const std::string& model_dir,
    std::shared_ptr<cvedix_nodes::cvedix_milvus_face_search_node> search_node)
{
    if (!std::filesystem::exists(dir_path)) {
        std::cerr << "Face registration directory not found: " << dir_path << std::endl;
        return 0;
    }

    // Initialize SeetaFace6 engines for feature extraction
    auto modelPath = [&](const std::string& name) { return model_dir + "/" + name; };
    auto device = seeta::ModelSetting::CPU;

    seeta::ModelSetting det_setting(modelPath("face_detector.csta"), device, 0);
    seeta::FaceDetector detector(det_setting);
    detector.set(seeta::FaceDetector::PROPERTY_MIN_FACE_SIZE, 40);

    seeta::ModelSetting lm_setting(modelPath("face_landmarker_pts5.csta"), device, 0);
    seeta::FaceLandmarker landmarker(lm_setting);

    seeta::ModelSetting rec_setting(modelPath("face_recognizer.csta"), device, 0);
    seeta::FaceRecognizer recognizer(rec_setting);

    int feature_size = recognizer.GetExtractFeatureSize();
    int registered = 0;

    std::cout << "\n── Registering faces from: " << dir_path << " ──\n";

    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" && ext != ".bmp") continue;

        std::string name = nameFromFilename(entry.path().filename().string());
        cv::Mat image = cv::imread(entry.path().string());
        if (image.empty()) continue;

        // Detect face
        SeetaImageData simg;
        simg.width = image.cols;
        simg.height = image.rows;
        simg.channels = image.channels();
        simg.data = image.data;

        SeetaFaceInfoArray faces = detector.detect(simg);
        if (faces.size == 0) {
            std::cerr << "  ✗ No face found in: " << entry.path().filename() << std::endl;
            continue;
        }

        // Use largest face
        int best_idx = 0;
        int best_area = 0;
        for (int i = 0; i < faces.size; i++) {
            int area = faces.data[i].pos.width * faces.data[i].pos.height;
            if (area > best_area) { best_area = area; best_idx = i; }
        }

        // Extract landmarks and features
        auto points = landmarker.mark(simg, faces.data[best_idx].pos);
        std::vector<float> features(feature_size);
        recognizer.Extract(simg, points.data(), features.data());

        // Register in Milvus
        int64_t id = search_node->registerFace(features, name);
        if (id >= 0) {
            std::cout << "  ✓ " << name << " → id=" << id 
                      << " (" << entry.path().filename() << ")" << std::endl;
            registered++;
        } else {
            std::cerr << "  ✗ Failed to register: " << name << std::endl;
        }
    }

    std::cout << "── Registered " << registered << " faces ──\n" << std::endl;
    return registered;
}

void print_usage(const char* program) {
    std::cout
        << "Usage: " << program << " [video] [model_dir] [milvus_uri] [register_dir] [port]\n"
        << "  video        : input video path\n"
        << "  model_dir    : SeetaFace6 model directory\n"
        << "  milvus_uri   : Milvus server URI (default: localhost:19530)\n"
        << "  register_dir : directory of face images to register (optional)\n"
        << "  port         : web dashboard port (default: 9093)\n\n"
        << "Pipeline:\n"
        << "  file_src → face_analysis → milvus_search → osd → web_debug\n\n"
        << "Prerequisites:\n"
        << "  docker compose -f configs/docker-compose.milvus.yml up -d\n\n"
        << "Example:\n"
        << "  " << program << " video.mp4 ./models/seetaface6 localhost:19530 ./faces/ 9093\n";
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

    // ── Parse arguments ──
    std::string video_path = first_existing({
        "../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./cvedix_data/videos/face.mp4",
        "../cvedix_data/videos/face.mp4"
    });
    std::string model_dir = first_existing({
        "../cvedix_data/models/seetaface6/sf3.0_models",
        "./cvedix_data/models/seetaface6/sf3.0_models"
    });
    std::string milvus_uri = "localhost:19530";
    std::string register_dir = "";
    int port = 9093;

    if (argc > 1) video_path = argv[1];
    if (argc > 2) model_dir = argv[2];
    if (argc > 3) milvus_uri = argv[3];
    if (argc > 4) register_dir = argv[4];
    if (argc > 5) port = std::stoi(argv[5]);

    if (!std::filesystem::exists(video_path)) {
        std::cerr << "Video not found: " << video_path << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    if (!std::filesystem::exists(model_dir)) {
        std::cerr << "Model directory not found: " << model_dir << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    CVEDIX_INFO("╔══════════════════════════════════════════════════════════════╗");
    CVEDIX_INFO("║  Milvus Large-Scale Face Search Demo                       ║");
    CVEDIX_INFO("╚══════════════════════════════════════════════════════════════╝");
    CVEDIX_INFO("Video:      " + video_path);
    CVEDIX_INFO("Models:     " + model_dir);
    CVEDIX_INFO("Milvus:     " + milvus_uri);
    CVEDIX_INFO("Register:   " + (register_dir.empty() ? "(none)" : register_dir));
    CVEDIX_INFO("Dashboard:  http://localhost:" + std::to_string(port));

    // ══════════════════════════════════════
    // 1. Create pipeline nodes
    // ══════════════════════════════════════

    // Source: file with loop
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6f, true);

    // Face analysis: detection + feature extraction (no local DB matching)
    auto analyzer = std::make_shared<cvedix_nodes::cvedix_face_analysis_node>(
        "face_analyzer",
        model_dir,
        "",               // empty db_path = no local recognition
        0.70f,            // similarity threshold (for local, not used here)
        80,               // min face size
        false,            // anti-spoofing OFF
        true,             // enable age/gender
        false,            // eye state OFF
        true              // enable pose estimation
    );
    analyzer->setFrameSkip(3);

    // Milvus face search: large-scale identification
    auto milvus_search = std::make_shared<cvedix_nodes::cvedix_milvus_face_search_node>(
        "milvus_search",
        milvus_uri,
        "cvedix_faces",    // collection name
        1024,              // SeetaFace6 embedding dimension
        0.70f,             // similarity threshold
        1,                 // top-1 match
        "IP"               // Inner Product metric
    );

    // OSD: rendering
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
    osd_config.bbox_color = cv::Scalar(0, 200, 100);
    osd_config.label_color = cv::Scalar(100, 255, 150);
    osd_config.label_font_scale = 0.5;
    osd_config.bbox_thickness = 2;
    osd->update_config(osd_config);

    // Web debug destination
    auto web_debug = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, port, nullptr, 50);

    // ══════════════════════════════════════
    // 2. Build pipeline
    // ══════════════════════════════════════
    analyzer->attach_to({file_src});
    milvus_search->attach_to({analyzer});
    osd->attach_to({milvus_search});
    web_debug->attach_to({osd});

    // ══════════════════════════════════════
    // 3. Register faces (before starting pipeline)
    // ══════════════════════════════════════
    if (!register_dir.empty()) {
        registerFacesFromDir(register_dir, model_dir, milvus_search);
    }

    // Print database stats
    int64_t db_size = milvus_search->getDatabaseSize();
    if (db_size >= 0) {
        CVEDIX_INFO("Milvus database: " + std::to_string(db_size) + " faces registered");
    }

    // ══════════════════════════════════════
    // 4. Analysis Board
    // ══════════════════════════════════════
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);
    web_debug->set_board(&board);

    // ══════════════════════════════════════
    // 5. Start pipeline
    // ══════════════════════════════════════
    file_src->start();

    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║  Milvus Large-Scale Face Search                            ║\n"
              << "║                                                            ║\n"
              << "║  Pipeline:                                                 ║\n"
              << "║    file_src → face_analysis → milvus_search → osd → web  ║\n"
              << "║                                                            ║\n"
              << "║  Milvus server: " << milvus_uri << "                              ║\n"
              << "║  Collection:    cvedix_faces                               ║\n"
              << "║  Database size: " << (db_size >= 0 ? std::to_string(db_size) : "N/A") << " faces                                    ║\n"
              << "║                                                            ║\n"
              << "║  Open in browser:                                          ║\n"
              << "║  → http://localhost:" << port << "                                    ║\n"
              << "║                                                            ║\n"
              << "║  Press Enter to stop...                                    ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << std::endl;

    std::string wait;
    std::getline(std::cin, wait);

    // Print final stats
    std::cout << "\n── Final Statistics ──\n"
              << "  Total searches: " << milvus_search->getTotalSearches() << "\n"
              << "  Total matches:  " << milvus_search->getTotalMatches() << "\n"
              << "  Avg latency:    " << milvus_search->getAvgSearchMs() << " ms\n"
              << std::endl;

    file_src->detach_recursively();
    return 0;
}

#else

#include <iostream>

int main() {
    std::cerr << "This sample requires both SeetaFace6 and Milvus SDK.\n"
              << "Build with: cmake -DCVEDIX_WITH_FACE=ON -DCVEDIX_WITH_MILVUS=ON ..\n\n"
              << "Steps:\n"
              << "  1. Build SeetaFace6: cd third_party/seetaface6 && bash build_seetaface6.sh\n"
              << "  2. Build Milvus SDK: cd third_party/milvus_sdk && bash build_milvus_sdk.sh\n"
              << "  3. Start Milvus: docker compose -f configs/docker-compose.milvus.yml up -d\n"
              << "  4. Rebuild: cmake -DCVEDIX_WITH_FACE=ON -DCVEDIX_WITH_MILVUS=ON ..\n";
    return 1;
}

#endif
