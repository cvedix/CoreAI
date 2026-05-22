/**
 * @file milvus_yolov11_face_register_sample.cpp
 * @brief YOLOv11 Face Detection + SeetaFace Embedding + SORT Tracking + Smart Milvus Registration
 *
 * Pipeline:
 *   file_src → yolov11_face → bridge (SeetaFace embeddings) → sort (FACE)
 *            → smart_register (Milvus) → exhibition_filter → osd → web_debug
 *
 * Features:
 *   - YOLOv11 for high-accuracy face detection (auto backend: TRT/ONNX/ORT)
 *   - SeetaFace6 for robust face embedding extraction (1024-dim)
 *   - SORT tracking to maintain face identity across frames
 *   - Smart registration: each track_id registers only ONCE per tracking session
 *   - When tracking is lost, re-appearance creates new registration (appearance log)
 *   - Each registration stores timestamp + track_id metadata for movement tracing
 *
 * Usage:
 *   ./milvus_yolov11_face_register_sample [video] [yolo_model] [seetaface_dir] [milvus_uri] [port]
 */

#if defined(CVEDIX_WITH_FACE) && defined(CVEDIX_WITH_MILVUS)

#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/infers/cvedix_milvus_vector_search_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

// SeetaFace6 for embedding extraction (not detection)
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/Common/Struct.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <chrono>
#include <random>
#include <mutex>
#include <string>

// ═══════════════════════════════════════════════════════════════
//  Helper
// ═══════════════════════════════════════════════════════════════
namespace {

std::string first_existing(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates)
        if (std::filesystem::exists(path)) return path;
    return candidates.empty() ? "" : candidates.front();
}

std::string ensure_face_labels_file() {
    const std::string p = "/tmp/cvedix_face_labels.txt";
    std::ofstream out(p, std::ios::trunc);
    if (out.is_open()) out << "face\n";
    return p;
}

static SeetaImageData cvMatToSeeta(const cv::Mat& mat) {
    SeetaImageData s;
    s.width = mat.cols;
    s.height = mat.rows;
    s.channels = mat.channels();
    s.data = mat.data;
    return s;
}

} // namespace

// Minimum face bbox size (pixels) for registration quality
// Faces smaller than this produce unreliable embeddings
static constexpr int MIN_FACE_SIZE = 60;  // 60x60 pixels minimum

// ═══════════════════════════════════════════════════════════════
//  Bridge Node: YOLO targets → face_targets with SeetaFace embeddings
// ═══════════════════════════════════════════════════════════════
class cvedix_yolo_face_bridge_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_yolo_face_bridge_node(std::string name, const std::string& model_dir)
        : cvedix_node(name), model_dir_(model_dir) {
        this->initialized();
    }

    ~cvedix_yolo_face_bridge_node() {
        delete recognizer_;
        delete landmarker_;
        deinitialized();
    }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initEngines()) return meta;
        if (meta->targets.empty() || meta->frame.empty()) return meta;

        SeetaImageData simg = cvMatToSeeta(meta->frame);

        for (auto& target : meta->targets) {
            SeetaRect rect;
            rect.x = std::max(0, target->x);
            rect.y = std::max(0, target->y);
            rect.width  = std::min(target->width,  meta->frame.cols - rect.x);
            rect.height = std::min(target->height, meta->frame.rows - rect.y);
            if (rect.width < MIN_FACE_SIZE || rect.height < MIN_FACE_SIZE) continue;

            // Landmarks (5-point)
            auto points = landmarker_->mark(simg, rect);
            std::vector<std::pair<int,int>> kps;
            kps.reserve(points.size());
            for (auto& pt : points)
                kps.emplace_back(static_cast<int>(pt.x), static_cast<int>(pt.y));

            // Extract embedding (1024-dim)
            int dim = recognizer_->GetExtractFeatureSize();
            std::vector<float> feat(dim);
            if (!recognizer_->Extract(simg, points.data(), feat.data())) continue;

            auto ft = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                rect.x, rect.y, rect.width, rect.height,
                target->primary_score, kps, feat);

            meta->face_targets.push_back(ft);
        }

        // Clear normal targets so OSD only renders face_targets
        meta->targets.clear();
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override { return meta; }

private:
    bool initEngines() {
        if (ready_) return true;
        try {
            auto mp = [&](const std::string& n){ return model_dir_ + "/" + n; };
            landmarker_ = new seeta::FaceLandmarker(seeta::ModelSetting(mp("face_landmarker_pts5.csta")));
            recognizer_ = new seeta::FaceRecognizer(seeta::ModelSetting(mp("face_recognizer.csta")));
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] SeetaFace6 bridge loaded (dim=%d)", node_name.c_str(), recognizer_->GetExtractFeatureSize()));
            ready_ = true;
            return true;
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] SeetaFace6 init failed: %s", node_name.c_str(), e.what()));
            return false;
        }
    }

    std::string model_dir_;
    seeta::FaceLandmarker* landmarker_ = nullptr;
    seeta::FaceRecognizer* recognizer_ = nullptr;
    bool ready_ = false;
    std::mutex mtx_;
};

// ═══════════════════════════════════════════════════════════════
//  Smart Register Node: Track-aware Milvus registration
//
//  Logic:
//    - Each track_id is registered only ONCE into Milvus
//    - If the face is already known (search hit), just assign name
//    - If unknown, auto-register with UUID + metadata
//    - When tracking is lost and a new track_id appears, it can register again
//      → creates an appearance history (multiple vectors per person)
// ═══════════════════════════════════════════════════════════════
class cvedix_smart_register_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_smart_register_node(std::string name,
                               std::shared_ptr<cvedix_nodes::cvedix_milvus_vector_search_node> milvus,
                               float threshold = 0.75f)
        : cvedix_node(name), milvus_(milvus), threshold_(threshold) {
        this->initialized();
    }

protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override
    {
        if (!milvus_ || meta->face_targets.empty()) return meta;

        for (auto& face : meta->face_targets) {
            if (face->embeddings.empty() || face->track_id < 0) continue;

            // Skip faces with bbox too small for reliable registration
            if (face->width < MIN_FACE_SIZE || face->height < MIN_FACE_SIZE) continue;

            int tid = face->track_id;

            // Already registered in this tracking session → just lookup name
            if (registered_.count(tid)) {
                auto res = milvus_->searchFace(face->embeddings, 1);
                if (!res.empty() && res[0].score >= threshold_) {
                    face->identify = res[0].name;
                    face->identify_score = res[0].score;
                }
                continue;
            }

            // First time seeing this track_id → search Milvus
            auto res = milvus_->searchFace(face->embeddings, 1);

            if (!res.empty() && res[0].score >= threshold_) {
                // Known face
                face->identify = res[0].name;
                face->identify_score = res[0].score;
                registered_.insert(tid);
            } else {
                // Unknown face → register with UUID
                std::string uuid = genUUID();

                int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                std::string md = "{\"track_id\":" + std::to_string(tid) +
                                 ",\"timestamp\":" + std::to_string(now) +
                                 ",\"frame_index\":" + std::to_string(meta->frame_index) + "}";

                int64_t id = milvus_->registerFace(face->embeddings, uuid, md);
                if (id >= 0) {
                    face->identify = uuid;
                    face->identify_score = 1.0f;
                    registered_.insert(tid);
                    total_++;
                    CVEDIX_INFO(cvedix_utils::string_format(
                        "[%s] Registered: %s (track=%d, milvus_id=%ld, total=%d)",
                        node_name.c_str(), uuid.c_str(), tid, id, total_));
                }
            }
        }
        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override { return meta; }

private:
    std::string genUUID() {
        std::uniform_int_distribution<int> d(0, 15);
        const char* v = "0123456789ABCDEF";
        std::string r = "Guest-";
        for (int i = 0; i < 8; i++) r += v[d(rng_)];
        return r;
    }

    std::shared_ptr<cvedix_nodes::cvedix_milvus_vector_search_node> milvus_;
    float threshold_;
    std::set<int> registered_;
    int total_ = 0;
    std::mt19937 rng_{std::random_device{}()};
};

// ═══════════════════════════════════════════════════════════════
//  Exhibition Filter: Format display text
// ═══════════════════════════════════════════════════════════════
class cvedix_register_display_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_register_display_node(std::string name) : cvedix_node(name) { this->initialized(); }
protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        for (auto& face : meta->face_targets) {
            if (face->identify.find("Guest-") == 0) {
                face->identify = (face->identify_score == 1.0f)
                    ? "[NEW] " + face->identify
                    : "[RETURNING] " + face->identify;
            } else if (!face->identify.empty() && face->identify != "Unknown") {
                face->identify = "[VIP] " + face->identify;
            }
            if (face->track_id >= 0)
                face->identify += " #" + std::to_string(face->track_id);
        }
        return meta;
    }
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override { return meta; }
};

// ═══════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::string video_path = first_existing({
        "../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4",
        "./cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4"
    });
    std::string face_model = first_existing({
        "./cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine",
        "../cvedix_data/models/tensorrt/face/yolov11-model-face-fp16.engine",
        "./cvedix_data/models/yolov11-model-face.onnx",
        "../cvedix_data/models/yolov11-model-face.onnx",
        "./cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml",
        "../cvedix_data/models/openvino/face/yolov11-model-face-fp16.xml"
    });
    std::string seetaface_dir = first_existing({
        "../cvedix_data/models/seetaface6/sf3.0_models",
        "./cvedix_data/models/seetaface6/sf3.0_models"
    });
    std::string milvus_uri = "localhost:19530";
    std::string collection = "cvedix_faces_yolov11";
    int port = 9095;

    if (argc > 1) video_path    = argv[1];
    if (argc > 2) face_model    = argv[2];
    if (argc > 3) seetaface_dir = argv[3];
    if (argc > 4) milvus_uri    = argv[4];
    if (argc > 5) port          = std::stoi(argv[5]);

    CVEDIX_INFO("═══════════════════════════════════════════════════════════");
    CVEDIX_INFO("  YOLOv11 Face Detection + Smart Milvus Registration     ");
    CVEDIX_INFO("═══════════════════════════════════════════════════════════");
    CVEDIX_INFO("Video:      " + video_path);
    CVEDIX_INFO("Face Model: " + face_model);
    CVEDIX_INFO("SeetaFace:  " + seetaface_dir);
    CVEDIX_INFO("Milvus:     " + milvus_uri);
    CVEDIX_INFO("Collection: " + collection);

    const std::string labels = ensure_face_labels_file();

    // ── Pipeline Nodes ──

    // 1. Source (loop video)
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6f, true);

    // 2. YOLOv11 Face Detector (multi-backend fallback)
    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>("yolov11_face");
    bool det_loaded = detector->load_model(
        face_model, cvedix_nodes::YoloVersion::YOLO11, labels,
        0.35f, 0.45f, 0, cvedix_nodes::BackendType::AUTO);

    // Fallback: try ONNX model with ORT backend
    if (!det_loaded) {
        std::string onnx = first_existing({
            "./cvedix_data/models/yolov11-model-face.onnx",
            "../cvedix_data/models/yolov11-model-face.onnx"});
        if (std::filesystem::exists(onnx)) {
            CVEDIX_INFO("Fallback: trying ORT backend with " + onnx);
            det_loaded = detector->load_model(
                onnx, cvedix_nodes::YoloVersion::YOLO11, labels,
                0.35f, 0.45f, 0, cvedix_nodes::BackendType::ORT);
        }
    }
    // Fallback: try ONNX model with OpenCV DNN backend
    if (!det_loaded) {
        std::string onnx = first_existing({
            "./cvedix_data/models/yolov11-model-face.onnx",
            "../cvedix_data/models/yolov11-model-face.onnx"});
        if (std::filesystem::exists(onnx)) {
            CVEDIX_INFO("Fallback: trying ONNX (OpenCV DNN) backend with " + onnx);
            det_loaded = detector->load_model(
                onnx, cvedix_nodes::YoloVersion::YOLO11, labels,
                0.35f, 0.45f, 0, cvedix_nodes::BackendType::ONNX);
        }
    }
    if (!det_loaded) {
        CVEDIX_ERROR("Failed to load YOLOv11 face model. Tried all backends.");
        return 1;
    }
    detector->set_allowed_classes({0}); // class 0 = face

    // 3. Bridge: YOLO targets → face_targets with SeetaFace6 embeddings (1024-dim)
    auto bridge = std::make_shared<cvedix_yolo_face_bridge_node>("face_bridge", seetaface_dir);

    // 4. SORT Tracker (FACE mode — tracks face_targets)
    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "face_tracker", cvedix_nodes::cvedix_track_for::FACE);

    // 5. Milvus client (standalone — used by smart_register for search/register API)
    auto milvus = std::make_shared<cvedix_nodes::cvedix_milvus_vector_search_node>(
        "milvus", milvus_uri, collection, 1024, 0.75f);

    // 6. Smart Register (tracking-aware: registers each track_id only once)
    auto smart_reg = std::make_shared<cvedix_smart_register_node>("smart_register", milvus, 0.75f);

    // 7. Display filter
    auto display = std::make_shared<cvedix_register_display_node>("display_filter");

    // 8. OSD
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_cfg;
    osd_cfg.show_bbox = true;
    osd_cfg.show_label = true;
    osd_cfg.show_track_id = true;
    osd_cfg.show_track_trail = true;
    osd_cfg.show_center_dot = true;
    osd_cfg.enable_face = true;
    osd_cfg.label_color = cv::Scalar(0, 255, 0);
    osd_cfg.bbox_color  = cv::Scalar(0, 255, 0);
    osd_cfg.bbox_thickness = 2;
    osd->update_config(osd_cfg);

    // 9. Web Debug
    auto web = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, port, nullptr, 50);

    // ── Build Pipeline ──
    detector->attach_to({file_src});
    bridge->attach_to({detector});
    tracker->attach_to({bridge});
    smart_reg->attach_to({tracker});
    display->attach_to({smart_reg});
    osd->attach_to({display});
    web->attach_to({osd});

    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);
    web->set_board(&board);

    // ── Start ──
    file_src->start();

    std::cout << "\n"
        << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║  YOLOv11 Face Registration + Milvus                        ║\n"
        << "║                                                            ║\n"
        << "║  Pipeline:                                                 ║\n"
        << "║    file_src → yolov11 → bridge → sort → register → osd    ║\n"
        << "║                                                            ║\n"
        << "║  Features:                                                 ║\n"
        << "║    • YOLOv11 face detection (auto-backend)                 ║\n"
        << "║    • SeetaFace6 embedding extraction (1024-dim)            ║\n"
        << "║    • SORT tracking (register once per track session)       ║\n"
        << "║    • Smart Milvus registration with appearance history     ║\n"
        << "║                                                            ║\n"
        << "║  Open: http://localhost:" << port << "                                 ║\n"
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
    std::cerr << "Requires CVEDIX_WITH_FACE=ON and CVEDIX_WITH_MILVUS=ON\n";
    return 1;
}
#endif
