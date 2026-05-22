/**
 * @file milvus_auto_register_sample.cpp
 * @brief Auto-register unknown faces into Milvus using UUIDs
 *
 * Pipeline:
 *   file_src → face_analysis (detection+extraction) → milvus_search (auto-register) → osd → web_debug_des
 *
 * Features:
 *   - Auto-generation of UUIDs for unknown faces
 *   - Real-time insertion into Milvus vector database
 *   - Seamless tracking of newly registered faces across frames
 */

#if defined(CVEDIX_WITH_FACE) && defined(CVEDIX_WITH_MILVUS)

#include "cvedix/nodes/infers/cvedix_face_analysis_node.h"
#include "cvedix/nodes/infers/cvedix_milvus_vector_search_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

#include <filesystem>
#include <iostream>
#include <string>

// Middleware node to filter faces by pose (only keep straight faces)
class cvedix_pose_filter_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_pose_filter_node(std::string name, float max_angle = 15.0f, int min_size = 120) 
        : cvedix_node(name), max_angle_(max_angle), min_size_(min_size) {
        this->initialized();
    }
protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        for (auto& face : meta->face_targets) {
            if (face->width < min_size_ || face->height < min_size_) {
                face->embeddings.clear(); // Prevent Milvus recognition/registration
                face->identify = "[TOO SMALL]";
            } else if (face->pose_valid) {
                if (std::abs(face->yaw) > max_angle_ || std::abs(face->pitch) > max_angle_) {
                    face->embeddings.clear(); // Prevent Milvus recognition/registration
                    face->identify = "[BAD POSE]";
                }
            }
        }
        return meta;
    }
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override {
        return meta;
    }
private:
    float max_angle_;
    int min_size_;
};

// Middleware node for Exhibition Mode
// Changes the display text based on whether the face is new or returning
class cvedix_exhibition_filter_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_exhibition_filter_node(std::string name) : cvedix_node(name) {
        this->initialized();
    }
protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        
        for (auto& face : meta->face_targets) {
            if (face->identify.find("Guest-") == 0) {
                // If score is exactly 1.0f, it was just registered by Milvus node this frame
                if (face->identify_score == 1.0f) {
                    face->identify = "[NEW] " + face->identify;
                } else {
                    face->identify = "[WELCOME BACK] " + face->identify;
                }
            } else if (!face->identify.empty() && face->identify != "Unknown" && face->identify != "[BAD POSE]" && face->identify != "[TOO SMALL]") {
                face->identify = "[VIP] " + face->identify;
            }
        }
        return meta;
    }
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override {
        return meta;
    }
};

// Middleware node to pad 512-dim mask embeddings to 1024-dim
// This allows Milvus to accept both standard (1024) and masked (512) faces in the same collection.
// Inner Product math still works because padded zeros contribute 0 to the dot product.
class cvedix_dimension_pad_node : public cvedix_nodes::cvedix_node {
public:
    cvedix_dimension_pad_node(std::string name) : cvedix_node(name) {
        this->initialized();
    }
protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
        for (auto& face : meta->face_targets) {
            if (face->embeddings.size() == 512) {
                face->embeddings.resize(1024, 0.0f);
            }
        }
        return meta;
    }
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override {
        return meta;
    }
};

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::string video_path = "../cvedix_data/videos/NVR5216-AI_ch9_main_20260115153800_20260115154559.mp4";
    std::string model_dir = "../cvedix_data/models/seetaface6/sf3.0_models";
    std::string milvus_uri = "localhost:19530";
    
    if (argc > 1) video_path = argv[1];
    if (argc > 2) model_dir = argv[2];

    CVEDIX_INFO("=========================================================");
    CVEDIX_INFO(" Milvus Auto-Registration Face Search Demo               ");
    CVEDIX_INFO("=========================================================");

    // 1. Source Node
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, video_path, 0.6f, true);

    // 2. Face Analysis Node (YOLOv11/YuNet for detection + SeetaFace for extraction)
    // Here we use cvedix_face_analysis_node as the standard CVEDIX face detector & extractor
    auto analyzer = std::make_shared<cvedix_nodes::cvedix_face_analysis_node>(
        "face_analyzer",
        model_dir,
        "",               // Empty local DB (we use Milvus instead)
        0.70f,            
        80,               // Min face size
        false, true, false, true  // enable_pose = true
    );
    analyzer->setFrameSkip(2); // Process every 2nd frame for performance

    // 3. Milvus Search Node
    auto milvus_search = std::make_shared<cvedix_nodes::cvedix_milvus_vector_search_node>(
        "milvus_search",
        milvus_uri,
        "cvedix_auto_faces_v2", // Dedicated collection
        1024,                // SeetaFace6 uses 1024-dim embeddings
        0.75f                // Threshold: higher to avoid false positives
    );
    
    // BẬT CHẾ ĐỘ AUTO REGISTER
    // Nếu khuôn mặt chưa tồn tại trong Milvus, node sẽ sinh một mã UUID (VD: a1b2c3d4-...)
    // và lập tức INSERT vào Milvus. Ở các frame sau, khuôn mặt này sẽ được nhận diện theo UUID đó.
    milvus_search->setAutoRegister(true);

    // 4. Exhibition Filter Node (Middleware to format OSD text)
    auto exhibition_filter = std::make_shared<cvedix_exhibition_filter_node>("exhibition_filter");

    // 5. OSD & Web Dashboard
    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    cvedix_nodes::unified_osd_config osd_config;
    osd_config.show_bbox = true;
    osd_config.show_label = true;
    osd_config.enable_face = true;
    osd_config.label_color = cv::Scalar(0, 255, 0); // Green labels for exhibition
    osd_config.bbox_color = cv::Scalar(0, 255, 0);
    osd_config.bbox_thickness = 3;
    osd->update_config(osd_config);

    auto web_debug = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
        "web_debug", 0, 9094, nullptr, 50);

    // Build Pipeline
    auto dimension_pad = std::make_shared<cvedix_dimension_pad_node>("dim_pad");
    auto pose_filter = std::make_shared<cvedix_pose_filter_node>("pose_filter", 15.0f, 60);
    
    analyzer->attach_to({file_src});
    pose_filter->attach_to({analyzer});
    dimension_pad->attach_to({pose_filter});
    milvus_search->attach_to({dimension_pad});
    exhibition_filter->attach_to({milvus_search});
    osd->attach_to({exhibition_filter});
    web_debug->attach_to({osd});

    cvedix_utils::cvedix_analysis_board board({file_src});
    board.push_to_buffer(5);
    web_debug->set_board(&board);

    // Bắt đầu chạy
    file_src->start();

    std::cout << "\n"
              << "Pipeline started! Open http://localhost:9094 in your browser.\n"
              << "Watch as unknown faces are automatically assigned UUIDs and saved to Milvus.\n"
              << "Press Enter to stop...\n" << std::endl;

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
