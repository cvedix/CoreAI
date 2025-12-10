#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_trt_insight_face_recognition_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "third_party/trt_insightface/util/algorithm_util.h"
// Note: Using cvedix_trt_insight_face_recognition_node instead of trt_insightface::InsightFaceRecognition
// to ensure license checking is performed

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>

/*
 * ============================================================================
 * InsightFace TensorRT Face Registration & Recognition Sample
 * ============================================================================
 * 
 * Mô tả:
 *   Sample này demo cách đăng ký khuôn mặt vào database và nhận diện 
 *   khuôn mặt từ video/image stream sử dụng InsightFace với TensorRT.
 * 
 * Chức năng:
 *   1. Đăng ký khuôn mặt từ ảnh vào database
 *   2. Nhận diện khuôn mặt trong video/image stream
 *   3. Hiển thị tên người được nhận diện
 * 
 * Usage:
 *   ./insightface_register_recognize_face_trt_sample [mode] [args...]
 * 
 *   Mode: register
 *     ./insightface_register_recognize_face_trt_sample register <image_path> <person_name>
 *     Ví dụ: ./insightface_register_recognize_face_trt_sample register alice.jpg "Alice"
 * 
 *   Mode: recognize
 *     ./insightface_register_recognize_face_trt_sample recognize [video_path|image_path]
 *     Ví dụ: ./insightface_register_recognize_face_trt_sample recognize face.mp4
 *     Ví dụ: ./insightface_register_recognize_face_trt_sample recognize photo.jpg
 * 
 * Database:
 *   Database được lưu trong file: ./face_database.txt
 *   Format: name|embedding1,embedding2,embedding3,...
 * 
 * Yêu cầu:
 *   - TensorRT engine: w600k_mbf_fp16_trt10.9.engine
 *   - Build với: cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON ..
 * 
 * ============================================================================
 */

static std::string get_project_root(const std::string& executable_path) {
    std::filesystem::path current = std::filesystem::absolute(executable_path).parent_path();
    
    for (int depth = 0; depth < 5; depth++) {
        if (current.filename() == "bin" && current.parent_path().filename() == "build") {
            return current.parent_path().parent_path().string();
        }
        if (std::filesystem::exists(current / "cvedix_data")) {
            return current.string();
        }
        if (current.has_parent_path() && current != current.parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }
    return std::filesystem::current_path().string();
}

static std::string resolve_path(const std::string& executable_path, const std::string& relative_path) {
    if (std::filesystem::path(relative_path).is_absolute()) {
        return relative_path;
    }
    
    std::string project_root = get_project_root(executable_path);
    std::filesystem::path full_path = std::filesystem::path(project_root) / relative_path;
    
    if (std::filesystem::exists(full_path)) return full_path.string();
    
    std::filesystem::path current_path = std::filesystem::current_path() / relative_path;
    if (std::filesystem::exists(current_path)) return current_path.string();
    
    return full_path.string();
}

// Face Database Class
class FaceDatabase {
private:
    std::map<std::string, std::vector<float>> database_;
    std::string db_file_path_;
    std::string project_root_;
    float threshold_ = 0.6f;
    std::shared_ptr<cvedix_nodes::cvedix_trt_insight_face_recognition_node> recognizer_node_;
    
    std::string resolve_model_path(const std::string& relative_path) {
        std::filesystem::path full_path = std::filesystem::path(project_root_) / relative_path;
        if (std::filesystem::exists(full_path)) return full_path.string();
        
        std::filesystem::path current_path = std::filesystem::current_path() / relative_path;
        if (std::filesystem::exists(current_path)) return current_path.string();
        if (std::filesystem::exists(relative_path)) return relative_path;
        
        return full_path.string();
    }

    void load_database() {
        std::ifstream file(db_file_path_);
        if (!file.is_open()) {
            std::ofstream create_file(db_file_path_);
            std::cout << "[DB] Creating new database" << std::endl;
            return;
        }

        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            size_t pos = line.find('|');
            if (pos == std::string::npos) continue;

            std::string name = line.substr(0, pos);
            std::string embedding_str = line.substr(pos + 1);
            std::vector<float> embedding;
            std::stringstream ss(embedding_str);
            std::string value;
            
            while (std::getline(ss, value, ',')) {
                embedding.push_back(std::stof(value));
            }

            // Accept any embedding size (not just 512)
            if (!embedding.empty()) {
                database_[name] = embedding;
                count++;
            }
        }
        std::cout << "[DB] Loaded " << count << " faces" << std::endl;
    }

    void save_database() {
        std::ofstream file(db_file_path_);
        if (!file.is_open()) {
            std::cerr << "[DB] Error: Cannot save to " << db_file_path_ << std::endl;
            return;
        }

        for (const auto& [name, embedding] : database_) {
            file << name << "|";
            for (size_t i = 0; i < embedding.size(); i++) {
                file << std::fixed << std::setprecision(6) << embedding[i];
                if (i < embedding.size() - 1) file << ",";
            }
            file << "\n";
        }
        std::cout << "[DB] Saved " << database_.size() << " faces" << std::endl;
    }

public:
    FaceDatabase(const std::string& executable_path, const std::string& db_path = "./face_database.txt", bool load_recognizer = true) 
        : project_root_(get_project_root(executable_path)),
          db_file_path_(resolve_path(executable_path, db_path)) {
        
        load_database();
        if (!load_recognizer) return;
        
        std::vector<std::string> engine_paths = {
            resolve_path(executable_path, "build/bin/cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine"),
            resolve_path(executable_path, "cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine"),
            (std::filesystem::path(project_root_) / "build/bin/cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine").string(),
            (std::filesystem::path(project_root_) / "cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine").string()
        };
        
        std::string engine_path = engine_paths[0];
        for (const auto& path : engine_paths) {
            if (std::filesystem::exists(path)) {
                engine_path = path;
                break;
            }
        }
        
        try {
            // Use cvedix_trt_insight_face_recognition_node instead of trt_insightface::InsightFaceRecognition
            // This ensures license checking is performed
            recognizer_node_ = std::make_shared<cvedix_nodes::cvedix_trt_insight_face_recognition_node>(
                "face_recognition_node", engine_path, 112, 112, false);  // disable alignment as we already align
        } catch (const std::runtime_error& e) {
            std::cerr << "[DB] Error: Failed to load engine: " << e.what() << std::endl;
            throw;  // Re-throw to be caught by caller
        } catch (const std::exception& e) {
            std::cerr << "[DB] Error: Failed to load engine: " << e.what() << std::endl;
            throw std::runtime_error(std::string("Failed to initialize face recognition: ") + e.what());
        }
    }

    bool register_face_from_image(const std::string& image_path, const std::string& person_name) {
        std::cout << "\n[Register] Image: " << image_path << ", Name: " << person_name << std::endl;

        if (!std::filesystem::exists(image_path)) {
            std::cerr << "[Register] Error: File not found" << std::endl;
            return false;
        }

        cv::Mat image = cv::imread(image_path);
        if (image.empty()) {
            std::cerr << "[Register] Error: Cannot read image" << std::endl;
            return false;
        }

        std::vector<std::string> detector_paths = {
            resolve_model_path("build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
            resolve_model_path("cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
            (std::filesystem::path(project_root_) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
            (std::filesystem::path(project_root_) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
        };
        
        std::string detector_model_path = detector_paths[0];
        for (const auto& path : detector_paths) {
            if (std::filesystem::exists(path)) {
                detector_model_path = path;
                break;
            }
        }

        cv::Ptr<cv::FaceDetectorYN> face_detector;
        try {
            face_detector = cv::FaceDetectorYN::create(
                detector_model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000,
                cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
        } catch (const std::exception& e) {
            std::cerr << "[Register] Error: Failed to create detector: " << e.what() << std::endl;
            return false;
        }
        
        if (face_detector.empty()) {
            std::cerr << "[Register] Error: Detector handle is empty" << std::endl;
            return false;
        }

        face_detector->setInputSize(image.size());
        cv::Mat faces;
        try {
            face_detector->detect(image, faces);
        } catch (const cv::Exception& e) {
            std::cerr << "[Register] Error: Detection failed: " << e.what() << std::endl;
            return false;
        }

        if (faces.rows == 0 || faces.empty()) {
            std::cerr << "[Register] Error: No face detected" << std::endl;
            return false;
        }

        float x = faces.at<float>(0, 0), y = faces.at<float>(0, 1);
        float w = faces.at<float>(0, 2), h = faces.at<float>(0, 3);
        float score = faces.at<float>(0, 14);

        x = std::max(0.0f, std::min(x, (float)(image.cols - 1)));
        y = std::max(0.0f, std::min(y, (float)(image.rows - 1)));
        w = std::max(1.0f, std::min(w, (float)(image.cols - x)));
        h = std::max(1.0f, std::min(h, (float)(image.rows - y)));

        std::cout << "[Register] Face: (" << (int)x << "," << (int)y << ") " 
                  << (int)w << "x" << (int)h << " (score: " << score << ")" << std::endl;

        cv::Mat face_roi = image(cv::Rect((int)x, (int)y, (int)w, (int)h)).clone();
        cv::Mat aligned_face;
        cv::resize(face_roi, aligned_face, cv::Size(112, 112));

        std::vector<cv::Mat> faces_vec = {aligned_face};
        std::vector<std::vector<float>> embeddings;
        recognizer_node_->extract_features(faces_vec, embeddings);

        if (embeddings.empty() || embeddings[0].empty()) {
            std::cerr << "[Register] Error: Failed to extract embedding" << std::endl;
            return false;
        }

        database_[person_name] = embeddings[0];
        save_database();
        std::cout << "[Register] ✓ Registered: " << person_name << std::endl;
        return true;
    }

    std::string identify(const std::vector<float>& query_embedding) {
        if (query_embedding.empty()) return "Unknown";

        std::string best_match = "Unknown";
        float best_sim = threshold_;

        for (const auto& [name, db_emb] : database_) {
            float sim = trt_insightface::util::cosine_similarity(query_embedding, db_emb);
            if (sim > best_sim) {
                best_sim = sim;
                best_match = name;
            }
        }

        return (best_match != "Unknown") 
            ? best_match + " (" + std::to_string(best_sim).substr(0, 4) + ")"
            : "Unknown";
    }

    size_t size() const { return database_.size(); }

    void list_all() {
        std::cout << "\n[DB] Registered (" << database_.size() << "):" << std::endl;
        for (const auto& [name, _] : database_) {
            std::cout << "  - " << name << std::endl;
        }
    }

    void set_threshold(float threshold) { threshold_ = threshold; }
};

// Global database instance
std::shared_ptr<FaceDatabase> g_database;

int recognize_image(const std::string& executable_path, const std::string& image_path);

void face_recognition_callback(std::string node_name, int queue_size, 
                               std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!frame_meta || frame_meta->face_targets.empty() || !g_database) return;

    for (auto& face : frame_meta->face_targets) {
        if (!face->embeddings.empty()) {
            std::string person_name = g_database->identify(face->embeddings);
            std::cout << "[Recognition] (" << face->x << "," << face->y 
                      << ") -> " << person_name << std::endl;
        }
    }
}

int recognize_image(const std::string& executable_path, const std::string& image_path) {
    std::cout << "\n[Image Recognition] Processing: " << image_path << std::endl;
    
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "[Error] Cannot read image" << std::endl;
        return 1;
    }
    
    std::string project_root = get_project_root(executable_path);
    std::vector<std::string> detector_paths = {
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
    };
    
    std::string detector_model_path = detector_paths[0];
    for (const auto& path : detector_paths) {
        if (std::filesystem::exists(path)) {
            detector_model_path = path;
            break;
        }
    }
    
    cv::Ptr<cv::FaceDetectorYN> face_detector;
    try {
        face_detector = cv::FaceDetectorYN::create(
            detector_model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000,
            cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        std::cerr << "[Error] Failed to create detector: " << e.what() << std::endl;
        return 1;
    }
    
    face_detector->setInputSize(image.size());
    cv::Mat faces;
    try {
        face_detector->detect(image, faces);
    } catch (const cv::Exception& e) {
        std::cerr << "[Error] Detection failed: " << e.what() << std::endl;
        return 1;
    }
    
    if (faces.rows == 0) {
        std::cout << "\n[Result] No faces detected" << std::endl;
        return 0;
    }
    
    std::vector<std::string> engine_paths = {
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine").string()
    };
    
    std::string engine_path = engine_paths[0];
    for (const auto& path : engine_paths) {
        if (std::filesystem::exists(path)) {
            engine_path = path;
            break;
        }
    }
    
    try {
        // Use cvedix_trt_insight_face_recognition_node instead of trt_insightface::InsightFaceRecognition
        // This ensures license checking is performed
        auto recognizer_node = std::make_shared<cvedix_nodes::cvedix_trt_insight_face_recognition_node>(
            "face_recognition_node", engine_path, 112, 112, false);  // disable alignment as we already align
        cv::Mat result_image = image.clone();
        std::cout << "\n[Results]" << std::endl;
        
        for (int i = 0; i < faces.rows; i++) {
            float x = faces.at<float>(i, 0), y = faces.at<float>(i, 1);
            float w = faces.at<float>(i, 2), h = faces.at<float>(i, 3);
            float score = faces.at<float>(i, 14);
            
            x = std::max(0.0f, std::min(x, (float)(image.cols - 1)));
            y = std::max(0.0f, std::min(y, (float)(image.rows - 1)));
            w = std::max(1.0f, std::min(w, (float)(image.cols - x)));
            h = std::max(1.0f, std::min(h, (float)(image.rows - y)));
            
            cv::Mat face_roi = image(cv::Rect((int)x, (int)y, (int)w, (int)h)).clone();
            cv::Mat aligned_face;
            cv::resize(face_roi, aligned_face, cv::Size(112, 112));
            
            std::vector<cv::Mat> faces_vec = {aligned_face};
            std::vector<std::vector<float>> embeddings;
            recognizer_node->extract_features(faces_vec, embeddings);
            
            if (embeddings.empty() || embeddings[0].empty()) continue;
            
            std::string person_name = g_database->identify(embeddings[0]);
            std::cout << "  Face " << (i+1) << ": (" << (int)x << "," << (int)y << ") " 
                      << (int)w << "x" << (int)h << " -> " << person_name << std::endl;
            
            cv::rectangle(result_image, cv::Rect((int)x, (int)y, (int)w, (int)h), cv::Scalar(0, 255, 0), 2);
            cv::putText(result_image, person_name, cv::Point((int)x, (int)y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 255, 0), 2);
        }
        
        cv::imwrite("recognition_result.jpg", result_image);
        std::cout << "\n[Output] Result saved to: recognition_result.jpg" << std::endl;
        return 0;
    } catch (const std::runtime_error& e) {
        std::cerr << "\n[Error] " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\n[Error] Unexpected error: " << e.what() << std::endl;
        return 1;
    }
}

int mode_register(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " register <image_path> <person_name>" << std::endl;
        return 1;
    }

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::cout << "\n=== Face Registration Mode ===" << std::endl;
    
    try {
        auto db = std::make_shared<FaceDatabase>(argv[0]);
        std::string resolved_image_path = resolve_path(argv[0], argv[2]);
        bool success = db->register_face_from_image(resolved_image_path, argv[3]);
        
        if (success) {
            db->list_all();
            std::cout << "\n✓ Registration completed!" << std::endl;
            return 0;
        }
        std::cerr << "\n✗ Registration failed!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "\n[Error] " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\n[Error] Unexpected error: " << e.what() << std::endl;
        return 1;
    }
}

bool is_image_file(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".tiff" || ext == ".webp");
}

int mode_recognize(int argc, char* argv[]) {
    std::string input_path = (argc >= 3) ? argv[2] : "./cvedix_data/test_video/face.mp4";

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::cout << "\n=== Face Recognition Mode ===" << std::endl;
    std::string resolved_input_path = resolve_path(argv[0], input_path);
    
    if (!std::filesystem::exists(resolved_input_path)) {
        std::cerr << "\n[Error] Input file not found: " << resolved_input_path << std::endl;
        return 1;
    }
    
    bool is_image = is_image_file(resolved_input_path);
    g_database = std::make_shared<FaceDatabase>(argv[0], "./face_database.txt", false);
    g_database->list_all();

    if (g_database->size() == 0) {
        std::cerr << "\n[Error] Database is empty! Register faces first." << std::endl;
        std::cerr << "Usage: " << argv[0] << " register <image_path> <person_name>" << std::endl;
        return 1;
    }

    std::cout << "\nConfig: Input=" << resolved_input_path 
              << ", Type=" << (is_image ? "Image" : "Video")
              << ", DB=" << g_database->size() << " faces, Threshold=0.6\n" << std::endl;
    
    if (is_image) {
        return recognize_image(argv[0], resolved_input_path);
    }

    std::string project_root = get_project_root(argv[0]);
    std::vector<std::string> detector_paths = {
        resolve_path(argv[0], "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        resolve_path(argv[0], "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
    };
    
    std::vector<std::string> engine_paths = {
        resolve_path(argv[0], "build/bin/cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine"),
        resolve_path(argv[0], "cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine").string()
    };
    
    std::string detector_model = detector_paths[0];
    for (const auto& path : detector_paths) {
        if (std::filesystem::exists(path)) {
            detector_model = path;
            break;
        }
    }
    
    std::string engine_model = engine_paths[0];
    for (const auto& path : engine_paths) {
        if (std::filesystem::exists(path)) {
            engine_model = path;
            break;
        }
    }

    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src", 0, resolved_input_path, 0.6);
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("detector", detector_model, 0.9f, 0.3f, 5000);
    auto recognizer = std::make_shared<cvedix_nodes::cvedix_trt_insight_face_recognition_node>("recognizer", engine_model, 112, 112, true);
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd");
    auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    recognizer->set_meta_handled_hooker(face_recognition_callback);
    detector->attach_to({file_src});
    recognizer->attach_to({detector});
    osd->attach_to({recognizer});
    screen->attach_to({osd});

    std::cout << "Pipeline: file_src → detector → recognizer → osd → screen" << std::endl;
    std::cout << "Starting... Press ENTER to stop\n" << std::endl;

    file_src->start();
    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    std::cout << "Stopped." << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:" << std::endl;
        std::cout << "  Register: " << argv[0] << " register <image_path> <person_name>" << std::endl;
        std::cout << "  Recognize: " << argv[0] << " recognize [video_path|image_path]" << std::endl;
        std::cout << "\nExamples:" << std::endl;
        std::cout << "  " << argv[0] << " register alice.jpg \"Alice\"" << std::endl;
        std::cout << "  " << argv[0] << " recognize face.mp4" << std::endl;
        std::cout << "  " << argv[0] << " recognize photo.jpg" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "register") return mode_register(argc, argv);
    if (mode == "recognize") return mode_recognize(argc, argv);
    
    std::cerr << "Unknown mode: " << mode << std::endl;
    std::cerr << "Valid modes: register, recognize" << std::endl;
    return 1;
}

