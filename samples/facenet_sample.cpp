/**
 * @file facenet_sample.cpp
 * @brief Sample application demonstrating FaceNet face recognition with cvedix
 * 
 * This sample shows how to:
 * 1. Detect faces using YuNet detector
 * 2. Extract face embeddings using FaceNet
 * 3. Match faces against a database
 * 4. Display results with OSD
 * 
 * Usage:
 *   ./facenet_sample <video_path> <face_database_path> [model_path]
 * 
 * Example:
 *   ./facenet_sample video.mp4 face_database.txt models/facenet_vggface2.onnx
 */

#include <iostream>
#include <fstream>
#include <map>
#include <cmath>
#include <memory>

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_facenet_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"

using namespace cvedix_nodes;
using namespace cvedix_objects;

/**
 * @brief Simple face database for matching
 */
class SimpleFaceDatabase {
public:
    std::map<std::string, std::vector<float>> embeddings;
    float threshold = 0.6f;  // Cosine similarity threshold
    
    /**
     * @brief Load face database from file
     * 
     * File format: Each line contains:
     * name embedding[0] embedding[1] ... embedding[511]
     */
    bool load(const std::string& path) {
        std::ifstream db(path);
        if (!db.is_open()) {
            CVEDIX_ERROR(cvedix_utils::string_format("Failed to open database: %s", path.c_str()));
            return false;
        }
        
        std::string name;
        int count = 0;
        while (db >> name) {
            std::vector<float> emb(512);
            for (int i = 0; i < 512; i++) {
                if (!(db >> emb[i])) {
                    CVEDIX_ERROR(cvedix_utils::string_format(
                        "Invalid embedding for %s at position %d", name.c_str(), i));
                    return false;
                }
            }
            
            // L2 normalize
            float norm = 0.0f;
            for (float val : emb) {
                norm += val * val;
            }
            norm = std::sqrt(norm);
            if (norm > 1e-6) {
                for (float& val : emb) {
                    val /= norm;
                }
            }
            
            embeddings[name] = emb;
            count++;
        }
        
        CVEDIX_INFO(cvedix_utils::string_format("Loaded %d faces from database", count));
        return true;
    }
    
    /**
     * @brief Compute cosine similarity between two embeddings
     */
    float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) {
            return -1.0f;
        }
        
        float dot = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            dot += a[i] * b[i];
        }
        
        // If embeddings are L2 normalized, dot product = cosine similarity
        return dot;
    }
    
    /**
     * @brief Find best matching face in database
     * 
     * @return pair of (name, similarity_score)
     */
    std::pair<std::string, float> find_match(const std::vector<float>& query_embedding) {
        if (embeddings.empty()) {
            return {"Unknown", 0.0f};
        }
        
        std::string best_name = "Unknown";
        float best_score = -1.0f;
        
        for (const auto& [name, db_emb] : embeddings) {
            float score = cosine_similarity(query_embedding, db_emb);
            if (score > best_score) {
                best_score = score;
                best_name = name;
            }
        }
        
        // Apply threshold
        if (best_score < threshold) {
            return {"Unknown", best_score};
        }
        
        return {best_name, best_score};
    }
    
    /**
     * @brief Add a new face to database
     */
    void add_face(const std::string& name, const std::vector<float>& embedding) {
        embeddings[name] = embedding;
        CVEDIX_INFO(cvedix_utils::string_format("Added face: %s", name.c_str()));
    }
    
    /**
     * @brief Save database to file
     */
    bool save(const std::string& path) {
        std::ofstream db(path);
        if (!db.is_open()) {
            CVEDIX_ERROR(cvedix_utils::string_format("Failed to save database: %s", path.c_str()));
            return false;
        }
        
        for (const auto& [name, emb] : embeddings) {
            db << name;
            for (float val : emb) {
                db << " " << val;
            }
            db << std::endl;
        }
        
        CVEDIX_INFO(cvedix_utils::string_format("Saved %d faces to database", 
            static_cast<int>(embeddings.size())));
        return true;
    }
};


int main(int argc, char** argv) {
    // Parse arguments
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <video_path> <face_database_path> [model_path]" << std::endl;
        std::cout << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  " << argv[0] << " video.mp4 face_database.txt" << std::endl;
        std::cout << "  " << argv[0] << " 0 face_database.txt models/facenet_vggface2.onnx" << std::endl;
        return 1;
    }
    
    std::string video_path = argv[1];
    std::string database_path = argv[2];
    std::string model_path = (argc > 3) ? argv[3] : "cvedix_data/models/face/facenet_vggface2.onnx";
    
    // Initialize logger
    cvedix_utils::cvedix_logger::get_instance().set_level(cvedix_utils::cvedix_log_level::INFO);
    
    CVEDIX_INFO("=== FaceNet Face Recognition Sample ===");
    CVEDIX_INFO(cvedix_utils::string_format("Video: %s", video_path.c_str()));
    CVEDIX_INFO(cvedix_utils::string_format("Database: %s", database_path.c_str()));
    CVEDIX_INFO(cvedix_utils::string_format("Model: %s", model_path.c_str()));
    
    try {
        // Load face database
        SimpleFaceDatabase face_db;
        if (!face_db.load(database_path)) {
            CVEDIX_WARN("Could not load face database, starting with empty database");
        }
        
        // 1. Create video source
        auto src = std::make_shared<cvedix_file_src_node>(
            "video_src",
            video_path,
            0,      // channel_index
            30,     // fps
            640,    // width
            480,    // height
            false   // loop
        );
        
        // 2. Create face detector (YuNet)
        auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
            "face_detector",
            "cvedix_data/models/face/face_detection_yunet_2023mar.onnx",
            640,    // input_width
            640,    // input_height
            0.6f,   // score_threshold
            0.3f    // nms_threshold
        );
        
        // 3. Create FaceNet recognition node
        auto facenet = std::make_shared<cvedix_facenet_node>(
            "facenet",
            model_path,
            160,          // input_width (FaceNet standard)
            160,          // input_height
            true,         // enable_alignment
            "vggface2"    // pretrained_dataset
        );
        
        // 4. Add hook to perform face matching
        facenet->add_hook([&face_db](std::shared_ptr<cvedix_frame_meta> meta) {
            for (auto& face : meta->face_targets) {
                if (!face->embeddings.empty()) {
                    // Find match in database
                    auto [name, score] = face_db.find_match(face->embeddings);
                    
                    // Update face metadata
                    face->primary_class_name = name;
                    face->confidence = score;
                    
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "Face matched: %s (score=%.3f)", name.c_str(), score));
                }
            }
        });
        
        // 5. Create OSD node for visualization
        auto osd = std::make_shared<cvedix_face_osd_node>("face_osd");
        
        // 6. Create display node
        auto display = std::make_shared<cvedix_screen_des_node>(
            "display",
            "FaceNet Face Recognition"
        );
        
        // Build pipeline
        CVEDIX_INFO("Building pipeline...");
        src->set_next({detector});
        detector->set_next({facenet});
        facenet->set_next({osd});
        osd->set_next({display});
        
        // Start nodes in reverse order
        CVEDIX_INFO("Starting pipeline...");
        display->start();
        osd->start();
        facenet->start();
        detector->start();
        src->start();
        
        CVEDIX_INFO("Pipeline started successfully!");
        CVEDIX_INFO("Press 'q' in display window to quit");
        
        // Wait for source to complete
        src->wait();
        
        // Print statistics
        CVEDIX_INFO("=== Statistics ===");
        CVEDIX_INFO(cvedix_utils::string_format(
            "Total faces processed: %d", facenet->total_faces_processed));
        
        // Optional: Save updated database
        // face_db.save(database_path);
        
        CVEDIX_INFO("Sample completed successfully");
        
    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format("Error: %s", e.what()));
        return 1;
    }
    
    return 0;
}






