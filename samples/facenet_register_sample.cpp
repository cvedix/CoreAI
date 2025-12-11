/**
 * @file facenet_register_sample.cpp
 * @brief Sample application for registering faces to database
 * 
 * This sample shows how to:
 * 1. Load images from directory
 * 2. Detect faces in images
 * 3. Extract embeddings using FaceNet
 * 4. Save to face database
 * 
 * Usage:
 *   ./facenet_register_sample <images_dir> <output_database> [model_path]
 * 
 * Directory structure:
 *   images_dir/
 *     ├── person1/
 *     │   ├── photo1.jpg
 *     │   └── photo2.jpg
 *     └── person2/
 *         ├── photo1.jpg
 *         └── photo2.jpg
 * 
 * Example:
 *   ./facenet_register_sample ./faces/ face_database.txt
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <opencv2/opencv.hpp>

#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_facenet_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"

namespace fs = std::filesystem;
using namespace cvedix_nodes;
using namespace cvedix_objects;

/**
 * @brief Register a single face image
 */
bool register_face_image(
    const std::string& person_name,
    const std::string& image_path,
    std::shared_ptr<cvedix_yunet_face_detector_node> detector,
    std::shared_ptr<cvedix_facenet_node> facenet,
    std::ofstream& db_file
) {
    // Load image
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        CVEDIX_ERROR(cvedix_utils::string_format("Failed to load image: %s", image_path.c_str()));
        return false;
    }
    
    CVEDIX_INFO(cvedix_utils::string_format("Processing: %s (%s)", 
        image_path.c_str(), person_name.c_str()));
    
    // Create frame meta
    auto frame_meta = std::make_shared<cvedix_frame_meta>();
    frame_meta->frame = image;
    frame_meta->channel_index = 0;
    frame_meta->frame_index = 0;
    
    // Detect faces
    std::vector<std::shared_ptr<cvedix_frame_meta>> batch = {frame_meta};
    // Note: In real implementation, you would call the node's processing method
    // For this sample, we'll simulate the process
    
    // For demonstration, let's assume we need to manually process
    // In production, you'd integrate with the full pipeline
    
    CVEDIX_WARN("This sample requires full pipeline integration - placeholder implementation");
    
    // TODO: Implement actual detection and recognition
    // detector->process_frame(frame_meta);
    // facenet->process_frame(frame_meta);
    
    // Check if face was detected
    if (frame_meta->face_targets.empty()) {
        CVEDIX_WARN(cvedix_utils::string_format("No face detected in: %s", image_path.c_str()));
        return false;
    }
    
    // Get first face embedding
    auto& face = frame_meta->face_targets[0];
    if (face->embeddings.empty()) {
        CVEDIX_ERROR(cvedix_utils::string_format("No embedding extracted from: %s", image_path.c_str()));
        return false;
    }
    
    // Verify embedding size
    if (face->embeddings.size() != 512) {
        CVEDIX_ERROR(cvedix_utils::string_format("Invalid embedding size: %d (expected 512)", 
            static_cast<int>(face->embeddings.size())));
        return false;
    }
    
    // Save to database
    db_file << person_name;
    for (float val : face->embeddings) {
        db_file << " " << val;
    }
    db_file << std::endl;
    
    CVEDIX_INFO(cvedix_utils::string_format("✓ Registered: %s", person_name.c_str()));
    
    // Optional: Save detected face image
    if (face->width > 0 && face->height > 0) {
        cv::Rect face_rect(face->x, face->y, face->width, face->height);
        // Clamp to image bounds
        face_rect.x = std::max(0, face_rect.x);
        face_rect.y = std::max(0, face_rect.y);
        face_rect.width = std::min(face_rect.width, image.cols - face_rect.x);
        face_rect.height = std::min(face_rect.height, image.rows - face_rect.y);
        
        if (face_rect.width > 0 && face_rect.height > 0) {
            cv::Mat face_crop = image(face_rect).clone();
            std::string output_path = "registered_faces/" + person_name + "_" + 
                                     fs::path(image_path).filename().string();
            cv::imwrite(output_path, face_crop);
        }
    }
    
    return true;
}

int main(int argc, char** argv) {
    // Parse arguments
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <images_dir> <output_database> [model_path]" << std::endl;
        std::cout << std::endl;
        std::cout << "Directory structure:" << std::endl;
        std::cout << "  images_dir/" << std::endl;
        std::cout << "    ├── person1/" << std::endl;
        std::cout << "    │   ├── photo1.jpg" << std::endl;
        std::cout << "    │   └── photo2.jpg" << std::endl;
        std::cout << "    └── person2/" << std::endl;
        std::cout << "        ├── photo1.jpg" << std::endl;
        std::cout << "        └── photo2.jpg" << std::endl;
        std::cout << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  " << argv[0] << " ./faces/ face_database.txt" << std::endl;
        return 1;
    }
    
    std::string images_dir = argv[1];
    std::string database_path = argv[2];
    std::string model_path = (argc > 3) ? argv[3] : "cvedix_data/models/face/facenet_vggface2.onnx";
    
    // Initialize logger
    cvedix_utils::cvedix_logger::get_instance().set_level(cvedix_utils::cvedix_log_level::INFO);
    
    CVEDIX_INFO("=== FaceNet Face Registration ===");
    CVEDIX_INFO(cvedix_utils::string_format("Images directory: %s", images_dir.c_str()));
    CVEDIX_INFO(cvedix_utils::string_format("Output database: %s", database_path.c_str()));
    CVEDIX_INFO(cvedix_utils::string_format("Model: %s", model_path.c_str()));
    
    // Check if images directory exists
    if (!fs::exists(images_dir) || !fs::is_directory(images_dir)) {
        CVEDIX_ERROR(cvedix_utils::string_format("Images directory not found: %s", images_dir.c_str()));
        return 1;
    }
    
    // Create output directory for registered faces
    fs::create_directories("registered_faces");
    
    try {
        // Create face detector
        auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
            "face_detector",
            "cvedix_data/models/face/face_detection_yunet_2023mar.onnx",
            640, 640, 0.6f, 0.3f
        );
        
        // Create FaceNet node
        auto facenet = std::make_shared<cvedix_facenet_node>(
            "facenet",
            model_path,
            160, 160, true, "vggface2"
        );
        
        // Open database file
        std::ofstream db_file(database_path);
        if (!db_file.is_open()) {
            CVEDIX_ERROR(cvedix_utils::string_format("Failed to create database: %s", database_path.c_str()));
            return 1;
        }
        
        // Supported image extensions
        std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".bmp"};
        
        // Process each person directory
        int total_registered = 0;
        int total_failed = 0;
        
        for (const auto& person_dir : fs::directory_iterator(images_dir)) {
            if (!person_dir.is_directory()) {
                continue;
            }
            
            std::string person_name = person_dir.path().filename().string();
            CVEDIX_INFO(cvedix_utils::string_format("\n--- Processing: %s ---", person_name.c_str()));
            
            // Process each image in person directory
            int person_count = 0;
            for (const auto& image_file : fs::directory_iterator(person_dir)) {
                if (!image_file.is_regular_file()) {
                    continue;
                }
                
                // Check extension
                std::string ext = image_file.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end()) {
                    continue;
                }
                
                // Register this face
                if (register_face_image(
                    person_name,
                    image_file.path().string(),
                    detector,
                    facenet,
                    db_file
                )) {
                    total_registered++;
                    person_count++;
                } else {
                    total_failed++;
                }
            }
            
            CVEDIX_INFO(cvedix_utils::string_format("Registered %d faces for %s", 
                person_count, person_name.c_str()));
        }
        
        db_file.close();
        
        // Print summary
        CVEDIX_INFO("\n=== Registration Summary ===");
        CVEDIX_INFO(cvedix_utils::string_format("Total registered: %d", total_registered));
        CVEDIX_INFO(cvedix_utils::string_format("Total failed: %d", total_failed));
        CVEDIX_INFO(cvedix_utils::string_format("Database saved to: %s", database_path.c_str()));
        
        if (total_registered == 0) {
            CVEDIX_WARN("No faces were registered!");
            return 1;
        }
        
        CVEDIX_INFO("Registration completed successfully");
        
    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format("Error: %s", e.what()));
        return 1;
    }
    
    return 0;
}






