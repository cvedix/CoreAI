/**
 * @file face_recognition_sample.cpp
 * @brief Sample demonstrating face recognition from image or video
 * 
 * Supports both IMAGE and VIDEO input (matches face_registration_sample)
 * 
 * Usage:
 *   ./face_recognition_sample <input_path>
 * 
 * Examples:
 *   ./face_recognition_sample ./photo.jpg          # Recognize from image
 *   ./face_recognition_sample ./video.mp4          # Recognize from video
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

namespace fs = std::filesystem;

// ========================================
// DATABASE CLASS (matches face_registration_sample format)
// ========================================

class FaceDatabase {
private:
    // Store multiple embeddings per person: vector of (name, embedding) pairs
    std::vector<std::pair<std::string, std::vector<float>>> database_;
    std::set<std::string> unique_names_;
    
    // Thresholds for ArcFace (512-dim) model
    float threshold_ = 0.55f;           // Minimum similarity to consider a match
    float high_confidence_ = 0.95f;     // Score above this = instant match (skip gap check)
    float ambiguity_threshold_ = 0.70f; // Only check gap if second score is above this
    float min_gap_ = 0.08f;             // Minimum gap when both are above ambiguity_threshold
    
public:
    bool load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "❌ Cannot open database: " << path << "\n";
            return false;
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
            
            if (!embedding.empty()) {
                database_.push_back({name, embedding});
                unique_names_.insert(name);
                count++;
            }
        }
        
        std::cout << "✓ Loaded " << count << " embeddings for " << unique_names_.size() << " persons\n";
        return count > 0;
    }
    
    std::pair<std::string, float> identify(const std::vector<float>& query_embedding) {
        if (query_embedding.empty() || database_.empty()) {
            return {"Unknown", 0.0f};
        }
        
        // Calculate max similarity for each person (may have multiple embeddings)
        std::map<std::string, float> person_max_sim;
        
        for (const auto& [name, db_emb] : database_) {
            if (query_embedding.size() != db_emb.size()) {
                continue;
            }
            
            // Cosine similarity
            float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
            for (size_t i = 0; i < query_embedding.size(); i++) {
                dot += query_embedding[i] * db_emb[i];
                norm_a += query_embedding[i] * query_embedding[i];
                norm_b += db_emb[i] * db_emb[i];
            }
            float sim = dot / (std::sqrt(norm_a) * std::sqrt(norm_b) + 1e-6f);
            
            // Keep max similarity for each person
            if (person_max_sim.find(name) == person_max_sim.end() || sim > person_max_sim[name]) {
                person_max_sim[name] = sim;
            }
        }
        
        if (person_max_sim.empty()) {
            return {"Unknown", 0.0f};
        }
        
        // Convert to vector and sort
        std::vector<std::pair<std::string, float>> similarities;
        for (const auto& [name, sim] : person_max_sim) {
            similarities.push_back({name, sim});
        }
        
        // Sort by similarity (descending)
        std::sort(similarities.begin(), similarities.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Debug: Print all similarities
        std::cout << "  [Debug] Similarity scores (max per person):\n";
        for (const auto& [name, sim] : similarities) {
            std::cout << "    - " << name << ": " << std::fixed << std::setprecision(4) << sim << "\n";
        }
        
        std::string best_match = similarities[0].first;
        float best_sim = similarities[0].second;
        
        // Check threshold
        if (best_sim < threshold_) {
            std::cout << "  [Debug] Below threshold (" << threshold_ << "), returning Unknown\n";
            return {"Unknown", best_sim};
        }
        
        // HIGH CONFIDENCE: If score is very high, match immediately (skip gap check)
        if (best_sim >= high_confidence_) {
            std::cout << "  [Debug] High confidence score (>=" << high_confidence_ << "), accepting match\n";
            return {best_match, best_sim};
        }
        
        // Confidence gap check: only relevant when second score is also high
        if (similarities.size() >= 2) {
            float second_sim = similarities[1].second;
            float gap = best_sim - second_sim;
            std::cout << "  [Debug] Gap between top-1 and top-2: " << std::fixed << std::setprecision(4) << gap << "\n";
            
            // Only check gap if second score is above ambiguity threshold
            // This prevents rejecting good matches just because another person has moderate similarity
            if (second_sim > ambiguity_threshold_) {
                std::cout << "  [Debug] Second score (" << second_sim << ") above ambiguity threshold (" << ambiguity_threshold_ << ")\n";
                if (gap < min_gap_) {
                    std::cout << "  [Debug] Gap too small (" << gap << " < " << min_gap_ << "), returning Unknown\n";
                    return {"Unknown", best_sim};
                }
            }
        }
        
        return {best_match, best_sim};
    }
    
    void list_all() {
        std::cout << "\n📋 Registered faces (" << unique_names_.size() << "):\n";
        for (const auto& name : unique_names_) {
            std::cout << "  - " << name << "\n";
        }
        std::cout << "\n";
    }
    
    size_t size() const { return unique_names_.size(); }
    
    int embedding_dim() const {
        if (database_.empty()) return 0;
        return database_[0].second.size();
    }
};

// ========================================
// HELPER FUNCTIONS
// ========================================

bool is_image_file(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".tiff" || ext == ".webp");
}

bool is_video_file(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || 
            ext == ".mov" || ext == ".wmv" || ext == ".flv");
}

// ========================================
// IMAGE RECOGNITION
// ========================================

int recognize_from_image(const std::string& image_path, const std::string& database_path) {
    std::cout << "\n========================================\n";
    std::cout << "📷 Image Recognition Mode\n";
    std::cout << "========================================\n";
    std::cout << "Image: " << image_path << "\n";
    std::cout << "Database: " << database_path << "\n";
    std::cout << "========================================\n\n";

    // Load database
    FaceDatabase db;
    if (!db.load(database_path)) {
        std::cerr << "❌ Database is empty or failed to load\n";
        return 1;
    }
    db.list_all();
    
    int expected_dim = db.embedding_dim();
    std::cout << "Expected embedding dimension: " << expected_dim << "\n\n";

    // Create face detector (use same model as registration)
    auto detector = cv::FaceDetectorYN::create(
        "./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx",
        "",
        cv::Size(320, 320),
        0.6f, 0.3f, 5000,
        cv::dnn::DNN_BACKEND_OPENCV,
        cv::dnn::DNN_TARGET_CPU
    );
    
    if (!detector) {
        std::cerr << "❌ Failed to create face detector\n";
        return 1;
    }
    std::cout << "✓ Created face detector\n";

    // Create face recognizer (use ArcFace w600k_mbf 512-dim for higher accuracy)
    auto recognizer = cv::FaceRecognizerSF::create(
        "./cvedix_data/models/face/face_recognition/w600k_mbf.onnx", ""
    );
    
    if (!recognizer) {
        std::cerr << "❌ Failed to create face recognizer\n";
        return 1;
    }
    std::cout << "✓ Created face recognizer\n\n";

    // Load image
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "❌ Failed to load image: " << image_path << "\n";
        return 1;
    }
    std::cout << "Processing image: " << image.cols << "x" << image.rows << "\n";

    // Detect faces
    detector->setInputSize(image.size());
    cv::Mat faces;
    detector->detect(image, faces);
    
    if (faces.rows == 0) {
        std::cout << "⚠️ No faces detected\n";
        return 0;
    }
    
    std::cout << "Found " << faces.rows << " face(s)\n\n";

    // Process each face
    cv::Mat result_image = image.clone();
    
    for (int i = 0; i < faces.rows; i++) {
        float x = faces.at<float>(i, 0);
        float y = faces.at<float>(i, 1);
        float w = faces.at<float>(i, 2);
        float h = faces.at<float>(i, 3);
        float score = faces.at<float>(i, 14);
        
        // Align face
        cv::Mat aligned_face;
        recognizer->alignCrop(image, faces.row(i), aligned_face);
        
        // Extract feature
        cv::Mat feature;
        recognizer->feature(aligned_face, feature);
        
        // Convert to vector
        std::vector<float> embedding(feature.begin<float>(), feature.end<float>());
        
        // Identify
        auto [name, similarity] = db.identify(embedding);
        
        // Draw results
        cv::Scalar color = (name != "Unknown") ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::rectangle(result_image, cv::Rect((int)x, (int)y, (int)w, (int)h), color, 2);
        
        std::string label = name + " (" + std::to_string(similarity).substr(0, 4) + ")";
        cv::putText(result_image, label, cv::Point((int)x, (int)y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.9, color, 2);
        
        // Print result
        if (name != "Unknown") {
            std::cout << "✅ Face " << (i+1) << ": " << name 
                      << " (similarity: " << std::fixed << std::setprecision(4) << similarity << ")\n";
        } else {
            std::cout << "❓ Face " << (i+1) << ": Unknown (best similarity: " 
                      << std::fixed << std::setprecision(4) << similarity << ")\n";
        }
    }

    // Save result image
    std::string output_path = "recognition_result.jpg";
    cv::imwrite(output_path, result_image);
    std::cout << "\n📷 Result saved to: " << output_path << "\n";

    return 0;
}

// ========================================
// VIDEO RECOGNITION (using pipeline)
// ========================================

int recognize_from_video(const std::string& video_path, const std::string& database_path) {
    std::cout << "\n========================================\n";
    std::cout << "🎬 Video Recognition Mode\n";
    std::cout << "========================================\n";
    std::cout << "Video: " << video_path << "\n";
    std::cout << "Database: " << database_path << "\n";
    std::cout << "========================================\n\n";

    // For video, we need to use OpenCV VideoCapture since pipeline nodes
    // use different model (512-dim). This keeps everything consistent.
    
    // Load database
    FaceDatabase db;
    if (!db.load(database_path)) {
        std::cerr << "❌ Database is empty or failed to load\n";
        return 1;
    }
    db.list_all();

    // Create detector and recognizer (same as image mode)
    auto detector = cv::FaceDetectorYN::create(
        "./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx",
        "", cv::Size(320, 320), 0.6f, 0.3f, 5000,
        cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU
    );
    
    auto recognizer = cv::FaceRecognizerSF::create(
        "./cvedix_data/models/face/face_recognition/w600k_mbf.onnx", ""
    );
    
    if (!detector || !recognizer) {
        std::cerr << "❌ Failed to create detector/recognizer\n";
        return 1;
    }
    
    std::cout << "✓ Created detector and recognizer\n";

    // Open video
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "❌ Failed to open video: " << video_path << "\n";
        return 1;
    }
    
    int frame_width = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int frame_height = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double fps = cap.get(cv::CAP_PROP_FPS);
    
    std::cout << "Video: " << frame_width << "x" << frame_height << " @ " << fps << " FPS\n";
    std::cout << "Press 'q' to quit, 's' to save current frame\n\n";

    cv::Mat frame;
    int frame_count = 0;
    
    while (cap.read(frame)) {
        frame_count++;
        
        // Detect faces
        detector->setInputSize(frame.size());
        cv::Mat faces;
        detector->detect(frame, faces);
        
        // Process each face
        for (int i = 0; i < faces.rows; i++) {
            float x = faces.at<float>(i, 0);
            float y = faces.at<float>(i, 1);
            float w = faces.at<float>(i, 2);
            float h = faces.at<float>(i, 3);
            
            // Align and extract feature
            cv::Mat aligned_face, feature;
            recognizer->alignCrop(frame, faces.row(i), aligned_face);
            recognizer->feature(aligned_face, feature);
            
            std::vector<float> embedding(feature.begin<float>(), feature.end<float>());
            auto [name, similarity] = db.identify(embedding);
            
            // Draw
            cv::Scalar color = (name != "Unknown") ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
            cv::rectangle(frame, cv::Rect((int)x, (int)y, (int)w, (int)h), color, 2);
            
            std::string label = name + " (" + std::to_string(similarity).substr(0, 4) + ")";
            cv::putText(frame, label, cv::Point((int)x, (int)y - 10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, color, 2);
            
            if (name != "Unknown" && frame_count % 30 == 0) {
                std::cout << "✅ " << name << " (" << std::fixed << std::setprecision(2) 
                          << similarity << ")\n";
            }
        }
        
        // Show frame info
        cv::putText(frame, "Frame: " + std::to_string(frame_count), cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        
        // Try to display (may fail without GUI)
        try {
            cv::imshow("Face Recognition", frame);
            int key = cv::waitKey(1);
            if (key == 'q' || key == 27) break;
            if (key == 's') {
                std::string save_path = "frame_" + std::to_string(frame_count) + ".jpg";
                cv::imwrite(save_path, frame);
                std::cout << "📷 Saved: " << save_path << "\n";
            }
        } catch (...) {
            // No GUI available, just process
            if (frame_count % 100 == 0) {
                std::cout << "Processing frame " << frame_count << "...\n";
            }
        }
    }
    
    cap.release();
    cv::destroyAllWindows();
    
    std::cout << "\nProcessed " << frame_count << " frames\n";
    return 0;
}

// ========================================
// MAIN
// ========================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Face Recognition Sample (supports both image and video)\n\n";
        std::cout << "Usage: " << argv[0] << " <input_path>\n\n";
        std::cout << "Examples:\n";
        std::cout << "  " << argv[0] << " ./photo.jpg           # Recognize from image\n";
        std::cout << "  " << argv[0] << " ./video.mp4           # Recognize from video\n";
        std::cout << "\nNote: Make sure to register faces first using face_registration_sample\n";
        return 1;
    }

    std::string input_path = argv[1];
    std::string database_path = "./face_database.txt";

    if (!fs::exists(input_path)) {
        std::cerr << "❌ Input file not found: " << input_path << "\n";
        return 1;
    }
    
    if (!fs::exists(database_path)) {
        std::cerr << "❌ Database not found: " << database_path << "\n";
        std::cerr << "Please register faces first using face_registration_sample\n";
        return 1;
    }

    if (is_image_file(input_path)) {
        return recognize_from_image(input_path, database_path);
    } else if (is_video_file(input_path)) {
        return recognize_from_video(input_path, database_path);
    } else {
        std::cerr << "❌ Unknown file type: " << input_path << "\n";
        std::cerr << "Supported: .jpg, .jpeg, .png, .bmp, .mp4, .avi, .mkv\n";
        return 1;
    }
}
