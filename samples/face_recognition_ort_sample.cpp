/**
 * @file face_recognition_ort_sample.cpp
 * @brief Face recognition sample using ONNX Runtime node (cvedix_face_recognition_ort_node)
 * 
 * This sample demonstrates using the cvedix_face_recognition_ort_node which uses
 * ONNX Runtime directly for face recognition, supporting models like glint360k_r100.
 * 
 * Usage:
 *   ./face_recognition_ort_sample <mode> [args]
 * 
 * Modes:
 *   register <image_path> <name>   - Register a face
 *   recognize <image_path>         - Recognize faces in image
 *   list                           - List registered faces
 * 
 * Examples:
 *   ./face_recognition_ort_sample register photo.jpg NGUYEN_VAN_A
 *   ./face_recognition_ort_sample recognize test.jpg
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <opencv2/opencv.hpp>

// Include the ORT node if available
#ifdef CVEDIX_WITH_ORT
#include "cvedix/nodes/infers/cvedix_face_recognition_ort_node.h"
#endif

namespace fs = std::filesystem;

// ========================================
// Face Database Helper (for registration)
// ========================================

class SimpleFaceDatabase {
private:
    std::vector<std::pair<std::string, std::vector<float>>> database_;
    std::set<std::string> unique_names_;
    std::string database_path_;
    
public:
    SimpleFaceDatabase(const std::string& path = "") : database_path_(path) {
        if (!path.empty() && fs::exists(path)) {
            load(path);
        }
    }
    
    bool load(const std::string& path) {
        database_path_ = path;
        std::ifstream file(path);
        if (!file.is_open()) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            size_t pos = line.find('|');
            if (pos == std::string::npos) continue;
            
            std::string name = line.substr(0, pos);
            std::string emb_str = line.substr(pos + 1);
            
            std::vector<float> embedding;
            std::stringstream ss(emb_str);
            std::string val;
            while (std::getline(ss, val, ',')) {
                embedding.push_back(std::stof(val));
            }
            
            if (!embedding.empty()) {
                database_.push_back({name, embedding});
                unique_names_.insert(name);
            }
        }
        
        std::cout << "✓ Loaded " << database_.size() << " embeddings for " 
                  << unique_names_.size() << " persons\n";
        return !database_.empty();
    }
    
    void save(const std::string& path = "") {
        std::string save_path = path.empty() ? database_path_ : path;
        if (save_path.empty()) save_path = "./face_database.txt";
        
        std::ofstream file(save_path);
        for (const auto& [name, emb] : database_) {
            file << name << "|";
            for (size_t i = 0; i < emb.size(); i++) {
                file << emb[i];
                if (i < emb.size() - 1) file << ",";
            }
            file << "\n";
        }
        std::cout << "✓ Saved " << database_.size() << " embeddings to " << save_path << "\n";
        database_path_ = save_path;
    }
    
    void add(const std::string& name, const std::vector<float>& embedding) {
        database_.push_back({name, embedding});
        unique_names_.insert(name);
    }
    
    void list() {
        std::cout << "\n📋 Registered faces (" << unique_names_.size() << "):\n";
        for (const auto& name : unique_names_) {
            int count = std::count_if(database_.begin(), database_.end(),
                [&name](const auto& p) { return p.first == name; });
            std::cout << "  - " << name << " (" << count << " embeddings)\n";
        }
    }
    
    size_t size() const { return unique_names_.size(); }
};

// ========================================
// Registration Mode (using ORT node)
// ========================================

#ifdef CVEDIX_WITH_ORT

int register_face(const std::string& image_path, const std::string& name,
                  const std::string& model_path, const std::string& detector_path) {
    
    std::cout << "\n========================================\n";
    std::cout << "📝 Registration Mode (ORT Node)\n";
    std::cout << "========================================\n";
    
    // Load face detector (YuNet)
    auto detector = cv::FaceDetectorYN::create(detector_path, "", cv::Size(320, 320),
        0.6f, 0.3f, 5000, cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
    if (!detector) {
        std::cerr << "❌ Failed to create face detector\n";
        return 1;
    }
    std::cout << "✓ Created face detector\n";
    
    // Create ORT face recognition node
    auto ort_node = std::make_shared<cvedix_nodes::cvedix_face_recognition_ort_node>(
        "face_rec_ort",
        model_path,
        "",  // No database for registration
        112, 112, true
    );
    std::cout << "✓ Created ORT face recognition node\n";
    
    // Load database
    SimpleFaceDatabase db("./face_database.txt");
    
    // Load image
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "❌ Failed to load image: " << image_path << "\n";
        return 1;
    }
    
    // Detect faces
    detector->setInputSize(image.size());
    cv::Mat faces;
    detector->detect(image, faces);
    
    if (faces.rows == 0) {
        std::cout << "⚠️ No faces detected\n";
        return 1;
    }
    
    std::cout << "Found " << faces.rows << " face(s)\n";
    
    int registered = 0;
    for (int i = 0; i < faces.rows; i++) {
        // Get 5-point landmarks from YuNet output
        float landmarks[5][2];
        for (int j = 0; j < 5; j++) {
            landmarks[j][0] = faces.at<float>(i, 4 + j * 2);
            landmarks[j][1] = faces.at<float>(i, 4 + j * 2 + 1);
        }
        
        // Align and extract embedding using ORT
        cv::Mat aligned = ort_node->alignFace(image, landmarks);
        std::vector<float> embedding = ort_node->extractEmbedding(aligned);
        
        db.add(name, embedding);
        registered++;
        
        std::cout << "✅ Registered face " << (i+1) << " for " << name << "\n";
    }
    
    db.save();
    std::cout << "\n📊 Total: " << registered << " embedding(s) registered for " << name << "\n";
    
    return 0;
}

int recognize_faces(const std::string& image_path,
                    const std::string& model_path, const std::string& detector_path) {
    
    std::cout << "\n========================================\n";
    std::cout << "🔍 Recognition Mode (ORT Node)\n";
    std::cout << "========================================\n";
    
    // Create ORT face recognition node with database
    auto ort_node = std::make_shared<cvedix_nodes::cvedix_face_recognition_ort_node>(
        "face_rec_ort",
        model_path,
        "./face_database.txt",
        cvedix_face_utils::RecognitionConfig::balanced(),
        112, 112
    );
    
    // Print database info
    ort_node->print_database_stats();
    
    // Load face detector
    auto detector = cv::FaceDetectorYN::create(detector_path, "", cv::Size(320, 320),
        0.6f, 0.3f, 5000, cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
    if (!detector) {
        std::cerr << "❌ Failed to create face detector\n";
        return 1;
    }
    std::cout << "✓ Created face detector\n";
    
    // Load image
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "❌ Failed to load image: " << image_path << "\n";
        return 1;
    }
    
    // Detect faces
    detector->setInputSize(image.size());
    cv::Mat faces;
    detector->detect(image, faces);
    
    if (faces.rows == 0) {
        std::cout << "⚠️ No faces detected\n";
        return 0;
    }
    
    std::cout << "\nFound " << faces.rows << " face(s)\n\n";
    
    cv::Mat result_image = image.clone();
    
    for (int i = 0; i < faces.rows; i++) {
        float x = faces.at<float>(i, 0);
        float y = faces.at<float>(i, 1);
        float w = faces.at<float>(i, 2);
        float h = faces.at<float>(i, 3);
        
        // Get landmarks
        float landmarks[5][2];
        for (int j = 0; j < 5; j++) {
            landmarks[j][0] = faces.at<float>(i, 4 + j * 2);
            landmarks[j][1] = faces.at<float>(i, 4 + j * 2 + 1);
        }
        
        // Align and extract embedding using ORT node
        cv::Mat aligned = ort_node->alignFace(image, landmarks);
        std::vector<float> embedding = ort_node->extractEmbedding(aligned);
        
        // Identify using node's database
        auto match = ort_node->get_database().find_match(embedding);
        
        // Draw results
        cv::Scalar color = match.confident ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::rectangle(result_image, cv::Rect((int)x, (int)y, (int)w, (int)h), color, 2);
        
        std::string label = match.name + " (" + std::to_string(match.score).substr(0, 5) + ")";
        cv::putText(result_image, label, cv::Point((int)x, (int)y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.9, color, 2);
        
        if (match.confident) {
            std::cout << "✅ Face " << (i+1) << ": " << match.name 
                      << " (similarity: " << std::fixed << std::setprecision(4) << match.score << ")\n";
        } else {
            std::cout << "❓ Face " << (i+1) << ": Unknown (best: " 
                      << std::fixed << std::setprecision(4) << match.score << ")\n";
        }
    }
    
    cv::imwrite("recognition_result.jpg", result_image);
    std::cout << "\n📷 Result saved to: recognition_result.jpg\n";
    
    return 0;
}

#else

int register_face(const std::string&, const std::string&,
                  const std::string&, const std::string&) {
    std::cerr << "❌ ONNX Runtime not available. Build with CVEDIX_WITH_ORT=ON\n";
    return 1;
}

int recognize_faces(const std::string&, const std::string&, const std::string&) {
    std::cerr << "❌ ONNX Runtime not available. Build with CVEDIX_WITH_ORT=ON\n";
    return 1;
}

#endif // CVEDIX_WITH_ORT

// ========================================
// Main
// ========================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Face Recognition with ONNX Runtime Node\n\n";
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << " register <image_path> <name>\n";
        std::cout << "  " << argv[0] << " recognize <image_path>\n";
        std::cout << "  " << argv[0] << " list\n";
        return 1;
    }
    
    std::string mode = argv[1];
    
    // Model paths - use glintr100 for higher accuracy
    std::string model_path = "./cvedix_data/models/face/face_recognition/glintr100.onnx";
    std::string detector_path = "./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx";
    
    if (mode == "register") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " register <image_path> <name>\n";
            return 1;
        }
        return register_face(argv[2], argv[3], model_path, detector_path);
        
    } else if (mode == "recognize") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " recognize <image_path>\n";
            return 1;
        }
        return recognize_faces(argv[2], model_path, detector_path);
        
    } else if (mode == "list") {
        SimpleFaceDatabase db("./face_database.txt");
        db.list();
        return 0;
        
    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }
}
