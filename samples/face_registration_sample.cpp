/**
 * @file face_registration_sample.cpp
 * @brief Sample demonstrating face registration with augmentation
 * 
 * Supports both IMAGE and VIDEO input:
 * 
 * Usage:
 *   ./face_registration_sample <input_path> <person_name>
 * 
 * Examples:
 *   ./face_registration_sample ./person.mp4 "John Doe"     # Video input
 *   ./face_registration_sample ./photo.jpg "Jane Smith"    # Image input
 *   ./face_registration_sample ./photos/ "Alice"           # Directory of images
 */

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_face_registration_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <iostream>
#include <filesystem>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

// ========================================
// HELPER FUNCTIONS
// ========================================

/**
 * @brief Check if file is an image
 */
bool is_image_file(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".tiff" || ext == ".webp");
}

/**
 * @brief Check if file is a video
 */
bool is_video_file(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || 
            ext == ".mov" || ext == ".wmv" || ext == ".flv" ||
            ext == ".webm" || ext == ".m4v");
}

/**
 * @brief Get all image files from directory
 */
std::vector<std::string> get_image_files(const std::string& dir_path) {
    std::vector<std::string> images;
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        return images;
    }
    
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && is_image_file(entry.path().string())) {
            images.push_back(entry.path().string());
        }
    }
    
    std::sort(images.begin(), images.end());
    return images;
}

// ========================================
// IMAGE REGISTRATION MODE
// ========================================

/**
 * @brief Register faces from image(s)
 */
int register_from_images(
    const std::vector<std::string>& image_paths,
    const std::string& person_name,
    const std::string& database_path
) {
    std::cout << "\n========================================\n";
    std::cout << "📷 Image Registration Mode\n";
    std::cout << "========================================\n";
    std::cout << "Images: " << image_paths.size() << "\n";
    std::cout << "Name: " << person_name << "\n";
    std::cout << "Database: " << database_path << "\n";
    std::cout << "Augmentation: Enabled (glasses, mask, hat)\n";
    std::cout << "========================================\n\n";

    // Create face detector (YuNet) with explicit backend/target to avoid DNN issues
    auto detector = cv::FaceDetectorYN::create(
        "./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx",
        "",
        cv::Size(320, 320),
        0.6f,  // score threshold (lower for better detection)
        0.3f,  // nms threshold
        5000,  // top_k
        cv::dnn::DNN_BACKEND_OPENCV,
        cv::dnn::DNN_TARGET_CPU
    );
    
    if (!detector) {
        std::cerr << "❌ Failed to create face detector\n";
        return 1;
    }
    
    std::cout << "✓ Created face detector\n";

    // Load face recognition model using FaceRecognizerSF (OpenCV's high-level API)
    std::string recog_model_path = "./cvedix_data/models/face/face_recognition/w600k_mbf.onnx";
    cv::Ptr<cv::FaceRecognizerSF> recognizer = cv::FaceRecognizerSF::create(
        recog_model_path, ""
    );
    
    if (!recognizer) {
        std::cerr << "❌ Failed to load face recognition model: " << recog_model_path << "\n";
        return 1;
    }
    
    std::cout << "✓ Loaded recognition model: " << recog_model_path << "\n";

    // Load augmentation templates
    cv::Mat glasses_template = cv::imread("./cvedix_data/face_assets/glasses_1.png", cv::IMREAD_UNCHANGED);
    cv::Mat mask_template = cv::imread("./cvedix_data/face_assets/mask_1.png", cv::IMREAD_UNCHANGED);
    cv::Mat hat_template = cv::imread("./cvedix_data/face_assets/hat_1.png", cv::IMREAD_UNCHANGED);

    bool has_glasses = !glasses_template.empty();
    bool has_mask = !mask_template.empty();
    bool has_hat = !hat_template.empty();
    
    std::cout << "Augmentation assets:\n";
    std::cout << "  Glasses: " << (has_glasses ? "✓" : "✗") << "\n";
    std::cout << "  Mask: " << (has_mask ? "✓" : "✗") << "\n";
    std::cout << "  Hat: " << (has_hat ? "✓" : "✗") << "\n\n";

    // Create directory to store registered face images
    std::string faces_dir = "./registered_faces";
    fs::create_directories(faces_dir);
    std::cout << "📁 Face images will be saved to: " << faces_dir << "/\n\n";

    // Open database file for appending
    std::ofstream db_file(database_path, std::ios::app);
    if (!db_file.is_open()) {
        std::cerr << "❌ Failed to open database file: " << database_path << "\n";
        return 1;
    }

    int total_registered = 0;
    int total_failed = 0;

    // Process each image
    for (const auto& image_path : image_paths) {
        std::cout << "Processing: " << image_path << "\n";
        
        cv::Mat image = cv::imread(image_path);
        if (image.empty()) {
            std::cerr << "  ⚠️ Failed to load image\n";
            total_failed++;
            continue;
        }

        // Detect faces
        try {
            detector->setInputSize(image.size());
        } catch (const cv::Exception& e) {
            std::cerr << "  ❌ setInputSize failed: " << e.what() << "\n";
            total_failed++;
            continue;
        }
        
        cv::Mat faces;
        try {
            detector->detect(image, faces);
        } catch (const cv::Exception& e) {
            std::cerr << "  ❌ detect failed: " << e.what() << "\n";
            total_failed++;
            continue;
        }
        
        if (faces.rows == 0) {
            std::cerr << "  ⚠️ No face detected\n";
            total_failed++;
            continue;
        }

        // Get face info for display
        float x = faces.at<float>(0, 0);
        float y = faces.at<float>(0, 1);
        float w = faces.at<float>(0, 2);
        float h = faces.at<float>(0, 3);
        float score = faces.at<float>(0, 14);

        std::cout << "  Face: (" << (int)x << "," << (int)y << ") " 
                  << (int)w << "x" << (int)h << " (score: " << score << ")\n";

        // Use FaceRecognizerSF's alignCrop for proper alignment
        cv::Mat aligned_face;
        try {
            recognizer->alignCrop(image, faces.row(0), aligned_face);
        } catch (const cv::Exception& e) {
            std::cerr << "  ❌ alignCrop failed: " << e.what() << "\n";
            total_failed++;
            continue;
        }

        // Lambda to extract embedding using FaceRecognizerSF
        auto extract_embedding = [&recognizer](const cv::Mat& face_img) -> std::vector<float> {
            // Ensure proper size
            cv::Mat input = face_img;
            if (input.rows != 112 || input.cols != 112) {
                cv::resize(face_img, input, cv::Size(112, 112), 0, 0, cv::INTER_LINEAR);
            }
            
            // Extract feature using FaceRecognizerSF
            cv::Mat feature;
            recognizer->feature(input, feature);
            
            if (feature.empty()) {
                return std::vector<float>();
            }
            
            // Convert to vector (feature is already normalized by FaceRecognizerSF)
            std::vector<float> embedding(feature.begin<float>(), feature.end<float>());
            
            return embedding;
        };

        // Create person directory for saving images
        std::string person_dir = faces_dir + "/" + person_name;
        fs::create_directories(person_dir);

        // Collect all embeddings for averaging
        std::vector<std::vector<float>> all_embeddings;

        // 1. Original face
        auto emb_original = extract_embedding(aligned_face);
        if (!emb_original.empty()) {
            all_embeddings.push_back(emb_original);
            cv::imwrite(person_dir + "/original.jpg", aligned_face);
        }

        // 2. Horizontal flip
        cv::Mat flipped;
        cv::flip(aligned_face, flipped, 1);
        auto emb_flip = extract_embedding(flipped);
        if (!emb_flip.empty()) {
            all_embeddings.push_back(emb_flip);
            cv::imwrite(person_dir + "/flip.jpg", flipped);
        }

        // 3. Brightness variations
        cv::Mat bright, dark;
        aligned_face.convertTo(bright, -1, 1.0, 15);
        aligned_face.convertTo(dark, -1, 1.0, -15);
        
        auto emb_bright = extract_embedding(bright);
        if (!emb_bright.empty()) {
            all_embeddings.push_back(emb_bright);
            cv::imwrite(person_dir + "/bright.jpg", bright);
        }
        
        auto emb_dark = extract_embedding(dark);
        if (!emb_dark.empty()) {
            all_embeddings.push_back(emb_dark);
            cv::imwrite(person_dir + "/dark.jpg", dark);
        }

        // 4. Contrast variation
        cv::Mat contrast;
        aligned_face.convertTo(contrast, -1, 1.1, 0);
        auto emb_contrast = extract_embedding(contrast);
        if (!emb_contrast.empty()) {
            all_embeddings.push_back(emb_contrast);
            cv::imwrite(person_dir + "/contrast.jpg", contrast);
        }

        // Also save the original source image crop
        int pad = 20;
        int crop_x = std::max(0, (int)x - pad);
        int crop_y = std::max(0, (int)y - pad);
        int crop_w = std::min((int)w + 2*pad, image.cols - crop_x);
        int crop_h = std::min((int)h + 2*pad, image.rows - crop_y);
        cv::Mat face_crop = image(cv::Rect(crop_x, crop_y, crop_w, crop_h)).clone();
        cv::imwrite(person_dir + "/source_crop.jpg", face_crop);

        // Skip if no embeddings were extracted
        if (all_embeddings.empty()) {
            std::cerr << "  ⚠️ Failed to extract any embeddings\n";
            total_failed++;
            continue;
        }

        // Average all embeddings
        size_t emb_dim = all_embeddings[0].size();
        std::vector<float> avg_embedding(emb_dim, 0.0f);
        
        for (const auto& emb : all_embeddings) {
            for (size_t i = 0; i < emb_dim; i++) {
                avg_embedding[i] += emb[i];
            }
        }
        
        float count = static_cast<float>(all_embeddings.size());
        for (size_t i = 0; i < emb_dim; i++) {
            avg_embedding[i] /= count;
        }
        
        // L2 normalize the averaged embedding
        float norm = 0.0f;
        for (float v : avg_embedding) {
            norm += v * v;
        }
        norm = std::sqrt(norm);
        if (norm > 1e-6) {
            for (float& v : avg_embedding) {
                v /= norm;
            }
        }

        // Save single averaged embedding to database
        db_file << person_name << "|";
        for (size_t i = 0; i < avg_embedding.size(); i++) {
            db_file << std::fixed << std::setprecision(6) << avg_embedding[i];
            if (i < avg_embedding.size() - 1) db_file << ",";
        }
        db_file << "\n";

        std::cout << "  ✅ Registered (averaged from " << all_embeddings.size() << " variants)\n";
        total_registered++;
    }

    db_file.close();

    std::cout << "\n========================================\n";
    std::cout << "📊 Registration Summary\n";
    std::cout << "========================================\n";
    std::cout << "Total embeddings registered: " << total_registered << "\n";
    std::cout << "Images processed: " << image_paths.size() << "\n";
    std::cout << "Failed: " << total_failed << "\n";
    std::cout << "Database saved to: " << database_path << "\n";
    std::cout << "========================================\n";

    return (total_registered > 0) ? 0 : 1;
}

// ========================================
// VIDEO REGISTRATION MODE
// ========================================

/**
 * @brief Register faces from video using pipeline
 */
int register_from_video(
    const std::string& video_path,
    const std::string& person_name,
    const std::string& database_path
) {
    std::cout << "\n========================================\n";
    std::cout << "🎬 Video Registration Mode\n";
    std::cout << "========================================\n";
    std::cout << "Video: " << video_path << "\n";
    std::cout << "Name: " << person_name << "\n";
    std::cout << "Database: " << database_path << "\n";
    std::cout << "Augmentation: Enabled (glasses, mask, hat)\n";
    std::cout << "========================================\n\n";

    // ========================================
    // CREATE NODES
    // ========================================

    // Video Source
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6
    );

    // Face Detector (YuNet)
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
        "detector", 
        "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
        0.9f, 0.3f
    );

    // Face Registration with augmentation
    auto registrar = std::make_shared<cvedix_nodes::cvedix_face_registration_node>(
        "registrar",
        "./cvedix_data/models/face/face_recognition/w600k_mbf.onnx",
        database_path,
        true,  // enable augmentation
        "./cvedix_data/face_assets/glasses_1.png",
        "./cvedix_data/face_assets/mask_1.png",
        "./cvedix_data/face_assets/hat_1.png",
        112, 112
    );

    // Set registration name
    registrar->set_registration_name(person_name);

    // OSD (visualization)
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd");

    // Screen Display
    auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    // ========================================
    // CONSTRUCT PIPELINE
    // ========================================

    detector->attach_to({file_src});
    registrar->attach_to({detector});
    osd->attach_to({registrar});
    screen->attach_to({osd});

    std::cout << "Pipeline: file_src → detector → registrar → osd → screen\n\n";

    // ========================================
    // ADD HOOK FOR RESULTS
    // ========================================
    
    registrar->set_meta_handled_hooker([&person_name](std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
        auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
        if (!frame_meta) return;
        
        for (auto& face : frame_meta->face_targets) {
            if (!face->embeddings.empty()) {
                std::cout << "📝 Registered embedding for " << person_name 
                          << " (dim=" << face->embeddings.size() << ")\n";
            }
        }
    });

    // ========================================
    // START PIPELINE
    // ========================================

    std::cout << "Starting pipeline...\n";
    std::cout << "Press ENTER to stop and save database\n\n";
    
    file_src->start();

    // Analysis board
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.display(1, false);

    // Wait for user input
    std::string wait;
    std::getline(std::cin, wait);

    // Cleanup
    std::cout << "\nStopping pipeline...\n";
    file_src->detach_recursively();
    
    // Save database
    if (registrar->save_database()) {
        std::cout << "✅ Database saved!\n";
        std::cout << "Registered " << registrar->get_last_registration_count() 
                  << " variants for " << person_name << "\n";
    }
    
    registrar->print_database_stats();
    std::cout << "Pipeline stopped.\n";

    return 0;
}

// ========================================
// MAIN
// ========================================

int main(int argc, char* argv[]) {
    // Configure logging
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    if (argc < 3) {
        std::cout << "Face Registration Sample (supports both image and video)\n\n";
        std::cout << "Usage: " << argv[0] << " <input_path> <person_name>\n\n";
        std::cout << "Examples:\n";
        std::cout << "  " << argv[0] << " ./person.mp4 \"John Doe\"     # Video input\n";
        std::cout << "  " << argv[0] << " ./photo.jpg \"Jane Smith\"    # Single image\n";
        std::cout << "  " << argv[0] << " ./photos/ \"Alice\"           # Directory of images\n";
        return 1;
    }

    std::string input_path = argv[1];
    std::string person_name = argv[2];
    std::string database_path = "./face_database.txt";

    // Check input type
    if (!fs::exists(input_path)) {
        std::cerr << "❌ Input path not found: " << input_path << "\n";
        return 1;
    }

    if (fs::is_directory(input_path)) {
        // Directory of images
        auto images = get_image_files(input_path);
        if (images.empty()) {
            std::cerr << "❌ No images found in directory: " << input_path << "\n";
            return 1;
        }
        return register_from_images(images, person_name, database_path);
    }
    else if (is_image_file(input_path)) {
        // Single image
        return register_from_images({input_path}, person_name, database_path);
    }
    else if (is_video_file(input_path)) {
        // Video file
        return register_from_video(input_path, person_name, database_path);
    }
    else {
        std::cerr << "❌ Unknown input type: " << input_path << "\n";
        std::cerr << "Supported: .jpg, .jpeg, .png, .bmp, .mp4, .avi, .mkv, .mov\n";
        return 1;
    }
}
