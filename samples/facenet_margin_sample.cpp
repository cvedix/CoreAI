/**
 * @file facenet_margin_sample.cpp
 * @brief Demonstration of margin-based confidence filtering for face recognition
 * 
 * This sample shows how margin checking improves face recognition accuracy
 * by rejecting ambiguous matches (where top-1 and top-2 are too close).
 * 
 * Based on Renesas AI accelerator project experience:
 * - Prefer false rejection over false acceptance
 * - Critical for poor lighting conditions
 * - Especially important when faces look similar
 * 
 * Usage:
 *   ./facenet_margin_sample <video_path> <face_database_path> [model_path]
 * 
 * Configuration:
 *   - similarity_threshold: 0.7 (min score to consider)
 *   - confidence_margin: 0.3 (min difference between top-1 and top-2)
 */

#include <iostream>
#include <memory>
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_facenet_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "face_database_with_margin.h"

using namespace cvedix_nodes;
using namespace cvedix_objects;
using namespace cvedix_face_utils;

/**
 * @brief Custom OSD node to display confidence metrics
 */
class MarginAwareFaceOSD : public cvedix_face_osd_node {
public:
    MarginAwareFaceOSD(const std::string& name) : cvedix_face_osd_node(name) {}
    
    virtual std::shared_ptr<cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_frame_meta> meta) override {
        
        // Call parent OSD
        cvedix_face_osd_node::handle_frame_meta(meta);
        
        // Add custom annotations for margin info
        for (auto& face : meta->face_targets) {
            if (face->custom_data.find("margin") != face->custom_data.end()) {
                float margin = std::stof(face->custom_data["margin"]);
                float second_score = std::stof(face->custom_data["second_score"]);
                std::string second_name = face->custom_data["second_name"];
                
                // Draw margin info below face box
                int text_y = face->y + face->height + 20;
                cv::Point text_pos(face->x, text_y);
                
                std::string margin_text = cvedix_utils::string_format(
                    "Margin: %.2f (2nd: %s %.2f)",
                    margin, second_name.c_str(), second_score);
                
                cv::putText(meta->frame, margin_text, text_pos,
                           cv::FONT_HERSHEY_SIMPLEX, 0.5,
                           cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
            }
        }
        
        return meta;
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Margin-based Face Recognition Demo" << std::endl;
        std::cout << std::endl;
        std::cout << "Usage: " << argv[0] << " <video_path> <face_database_path> [model_path]" << std::endl;
        std::cout << std::endl;
        std::cout << "This demo shows how confidence margin reduces false positives:" << std::endl;
        std::cout << "  - Similarity threshold: 0.7" << std::endl;
        std::cout << "  - Confidence margin: 0.3" << std::endl;
        std::cout << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  " << argv[0] << " video.mp4 face_database.txt" << std::endl;
        std::cout << std::endl;
        std::cout << "Decision logic:" << std::endl;
        std::cout << "  if (top1_score >= 0.7 AND (top1 - top2) >= 0.3):" << std::endl;
        std::cout << "      ACCEPT match" << std::endl;
        std::cout << "  else:" << std::endl;
        std::cout << "      REJECT (return 'Unknown')" << std::endl;
        return 1;
    }
    
    std::string video_path = argv[1];
    std::string database_path = argv[2];
    std::string model_path = (argc > 3) ? argv[3] : "cvedix_data/models/face/facenet_vggface2.onnx";
    
    // Initialize logger
    cvedix_utils::cvedix_logger::get_instance().set_level(cvedix_utils::cvedix_log_level::INFO);
    
    CVEDIX_INFO("=== FaceNet with Margin-based Confidence Filtering ===");
    CVEDIX_INFO(cvedix_utils::string_format("Video: %s", video_path.c_str()));
    CVEDIX_INFO(cvedix_utils::string_format("Database: %s", database_path.c_str()));
    CVEDIX_INFO(cvedix_utils::string_format("Model: %s", model_path.c_str()));
    
    try {
        // Load face database with margin checking
        EnhancedFaceDatabase face_db;
        face_db.similarity_threshold = 0.7f;   // Standard threshold
        face_db.confidence_margin = 0.3f;      // Margin between top-1 and top-2
        face_db.use_margin_check = true;       // Enable margin checking
        face_db.strict_mode = true;            // Require BOTH threshold AND margin
        
        if (!face_db.load(database_path, true)) {
            CVEDIX_ERROR("Failed to load face database");
            return 1;
        }
        
        CVEDIX_INFO("=== Configuration ===");
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Similarity threshold: %.2f", face_db.similarity_threshold));
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Confidence margin: %.2f", face_db.confidence_margin));
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Margin check: %s", face_db.use_margin_check ? "ENABLED" : "DISABLED"));
        CVEDIX_INFO(cvedix_utils::string_format(
            "  Mode: %s", face_db.strict_mode ? "STRICT" : "RELAXED"));
        
        // Create video source
        auto src = std::make_shared<cvedix_file_src_node>(
            "video_src", video_path, 0, 30, 640, 480, false);
        
        // Create face detector
        auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
            "face_detector",
            "cvedix_data/models/face/face_detection_yunet_2023mar.onnx",
            640, 640, 0.6f, 0.3f);
        
        // Create FaceNet recognition node
        auto facenet = std::make_shared<cvedix_facenet_node>(
            "facenet", model_path, 160, 160, true, "vggface2");
        
        // Add hook for margin-based matching
        facenet->add_hook([&face_db](std::shared_ptr<cvedix_frame_meta> meta) {
            for (auto& face : meta->face_targets) {
                if (!face->embeddings.empty()) {
                    // Find match with margin checking
                    MatchResult result = face_db.find_match(face->embeddings);
                    
                    // Update face metadata
                    face->primary_class_name = result.name;
                    face->confidence = result.score;
                    
                    // Store margin info for OSD display
                    face->custom_data["margin"] = std::to_string(result.margin);
                    face->custom_data["confident"] = result.confident ? "yes" : "no";
                    face->custom_data["second_name"] = result.second_name;
                    face->custom_data["second_score"] = std::to_string(result.second_score);
                    
                    // Log detailed info
                    if (!result.confident) {
                        CVEDIX_WARN(cvedix_utils::string_format(
                            "⚠ Ambiguous match REJECTED: top1=%s(%.3f) vs top2=%s(%.3f), margin=%.3f < 0.3",
                            result.name.c_str(), result.score,
                            result.second_name.c_str(), result.second_score,
                            result.margin));
                    } else {
                        CVEDIX_INFO(cvedix_utils::string_format(
                            "✓ Confident match: %s (score=%.3f, margin=%.3f)",
                            result.name.c_str(), result.score, result.margin));
                    }
                }
            }
        });
        
        // Create custom OSD with margin display
        auto osd = std::make_shared<MarginAwareFaceOSD>("face_osd");
        
        // Create display node
        auto display = std::make_shared<cvedix_screen_des_node>(
            "display", "FaceNet with Margin Check");
        
        // Build pipeline
        CVEDIX_INFO("Building pipeline...");
        src->set_next({detector});
        detector->set_next({facenet});
        facenet->set_next({osd});
        osd->set_next({display});
        
        // Start pipeline
        CVEDIX_INFO("Starting pipeline...");
        display->start();
        osd->start();
        facenet->start();
        detector->start();
        src->start();
        
        CVEDIX_INFO("=== Controls ===");
        CVEDIX_INFO("  Press 'q' to quit");
        CVEDIX_INFO("  Watch for rejection warnings in logs");
        
        // Wait for completion
        src->wait();
        
        // Print statistics
        CVEDIX_INFO("\n=== Final Statistics ===");
        face_db.print_statistics();
        
        CVEDIX_INFO(cvedix_utils::string_format(
            "Total faces processed: %d", facenet->total_faces_processed));
        
        CVEDIX_INFO("\n=== Analysis ===");
        CVEDIX_INFO("Margin checking benefits:");
        CVEDIX_INFO("  ✓ Reduces false positives (wrong person identification)");
        CVEDIX_INFO("  ✓ Better for similar-looking people");
        CVEDIX_INFO("  ✓ More robust in poor lighting conditions");
        CVEDIX_INFO("  ✓ Preferred for high-security applications");
        CVEDIX_INFO("");
        CVEDIX_INFO("Trade-offs:");
        CVEDIX_INFO("  ⚠ May increase false negatives (reject correct person)");
        CVEDIX_INFO("  ⚠ Need to tune margin parameter for your use case");
        
        CVEDIX_INFO("\nSample completed successfully");
        
    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format("Error: %s", e.what()));
        return 1;
    }
    
    return 0;
}






