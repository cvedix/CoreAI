/**
 * @file cvedix_face_recognition_ort_node.cpp
 * @brief Face recognition node using ONNX Runtime implementation
 * 
 * This node provides face recognition using ONNX Runtime directly instead of OpenCV DNN,
 * enabling use of advanced InsightFace models like glint360k_r100.
 * 
 * @author CVEDIX Team
 * @date 2024
 */

#ifdef CVEDIX_WITH_ORT

#include "cvedix_face_recognition_ort_node.h"
#ifdef CVEDIX_WITH_LICENSE
#include "cvedix/utils/license/cvedix_license_manager.h"
#endif
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

namespace cvedix_nodes {

// ========================================
// OrtFaceRecognizer Implementation
// ========================================

OrtFaceRecognizer::OrtFaceRecognizer(const std::string& model_path)
    : env_(ORT_LOGGING_LEVEL_WARNING, "FaceRecognizer") {
    
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(4);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    
    session_ = Ort::Session(env_, model_path.c_str(), session_options);
    
    // Get input info
    size_t num_inputs = session_.GetInputCount();
    for (size_t i = 0; i < num_inputs; i++) {
        auto name = session_.GetInputNameAllocated(i, allocator_);
        input_names_.push_back(strdup(name.get()));
        
        auto type_info = session_.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        input_shape_ = tensor_info.GetShape();
    }
    
    // Get output info
    size_t num_outputs = session_.GetOutputCount();
    for (size_t i = 0; i < num_outputs; i++) {
        auto name = session_.GetOutputNameAllocated(i, allocator_);
        output_names_.push_back(strdup(name.get()));
        
        auto type_info = session_.GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();
        if (shape.size() >= 2) {
            embedding_dim_ = shape[1];
        }
    }
    
    CVEDIX_INFO(cvedix_utils::string_format(
        "[OrtFaceRecognizer] Loaded ONNX model: %s (emb_dim=%d)",
        model_path.c_str(), embedding_dim_));
}

OrtFaceRecognizer::~OrtFaceRecognizer() {
    for (auto name : input_names_) free((void*)name);
    for (auto name : output_names_) free((void*)name);
}

cv::Mat OrtFaceRecognizer::getSimilarityTransformMatrix(float src[5][2]) {
    float dst[5][2];
    for (int i = 0; i < 5; i++) {
        dst[i][0] = arcface_dst_[i][0];
        dst[i][1] = arcface_dst_[i][1];
    }
    
    cv::Mat src_mat(5, 2, CV_32F, src);
    cv::Mat dst_mat(5, 2, CV_32F, dst);
    
    return cv::estimateAffinePartial2D(src_mat, dst_mat);
}

cv::Mat OrtFaceRecognizer::alignCrop(const cv::Mat& src_img, float landmarks[5][2]) {
    cv::Mat transform = getSimilarityTransformMatrix(landmarks);
    cv::Mat aligned;
    cv::warpAffine(src_img, aligned, transform, cv::Size(112, 112));
    return aligned;
}

std::vector<float> OrtFaceRecognizer::extractEmbedding(const cv::Mat& aligned_face) {
    // Prepare input: BGR -> RGB, then normalize to [-1, 1]
    cv::Mat rgb;
    cv::cvtColor(aligned_face, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F);
    
    // InsightFace preprocessing: (pixel - 127.5) / 127.5 -> range [-1, 1]
    rgb = (rgb - 127.5f) / 127.5f;
    
    // Create input tensor [1, 3, 112, 112]
    std::vector<float> input_data(1 * 3 * 112 * 112);
    
    // Convert HWC to CHW
    for (int c = 0; c < 3; c++) {
        for (int h = 0; h < 112; h++) {
            for (int w = 0; w < 112; w++) {
                input_data[c * 112 * 112 + h * 112 + w] = rgb.at<cv::Vec3f>(h, w)[c];
            }
        }
    }
    
    // Create ONNX tensor
    std::vector<int64_t> input_shape = {1, 3, 112, 112};
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_data.data(), input_data.size(),
        input_shape.data(), input_shape.size()
    );
    
    // Run inference
    auto output_tensors = session_.Run(
        Ort::RunOptions{nullptr},
        input_names_.data(), &input_tensor, 1,
        output_names_.data(), output_names_.size()
    );
    
    // Get output embedding
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    std::vector<float> embedding(output_data, output_data + embedding_dim_);
    
    // L2 normalize
    float norm = 0.0f;
    for (float v : embedding) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-6f) {
        for (float& v : embedding) v /= norm;
    }
    
    return embedding;
}

// ========================================
// cvedix_face_recognition_ort_node Implementation
// ========================================

cvedix_face_recognition_ort_node::cvedix_face_recognition_ort_node(
    std::string node_name,
    std::string model_path,
    std::string database_path,
    int input_width,
    int input_height,
    bool enable_alignment)
    : cvedix_face_recognition_ort_node(
        node_name, model_path, database_path,
        cvedix_face_utils::RecognitionConfig::balanced(),
        input_width, input_height) {
    this->enable_alignment = enable_alignment;
}

cvedix_face_recognition_ort_node::cvedix_face_recognition_ort_node(
    std::string node_name,
    std::string model_path,
    std::string database_path,
    const cvedix_face_utils::RecognitionConfig& config,
    int input_width,
    int input_height)
    : cvedix_secondary_infer_node(node_name, model_path, "", "",
                                   input_width, input_height,
                                   1, std::vector<int>(), 0, 0,
                                   0, 1.0f,
                                   cv::Scalar(0, 0, 0),
                                   cv::Scalar(1, 1, 1),
                                   true, false),
      enable_alignment(true),
      database_path_(database_path),
      config_(config) {

    #ifdef CVEDIX_WITH_LICENSE
    if (!cvedix_utils::cvedix_license_manager::get_instance().check_license()) {
        throw std::runtime_error("Face recognition features require a valid license.");
    }
    #endif

    // Initialize voting buffer if enabled
    if (config_.voting_enabled) {
        voting_buffer_ = std::make_unique<cvedix_face_utils::VotingBuffer>(
            config_.voting_window_size,
            config_.voting_majority_threshold
        );
    }

    // Apply config
    similarity_threshold = config_.similarity_threshold;
    confidence_margin = config_.confidence_margin;

    // Load ONNX Runtime model
    try {
        ort_recognizer_ = std::make_unique<OrtFaceRecognizer>(model_path);
        embedding_size_ = ort_recognizer_->getEmbeddingDim();
        
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Loaded ORT model: %s (emb=%d, voting=%s, tta=%s)",
            node_name.c_str(), model_path.c_str(), embedding_size_,
            config_.voting_enabled ? "ON" : "OFF",
            config_.tta.enabled ? "ON" : "OFF"));

    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Exception loading ORT model: %s", node_name.c_str(), e.what()));
        throw;
    }

    // Configure database
    database_.similarity_threshold = similarity_threshold;
    database_.confidence_margin = confidence_margin;

    // Load database
    if (!database_path.empty()) {
        if (load_database(database_path)) {
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Loaded database: %s", node_name.c_str(), database_path.c_str()));
        }
    }

    this->initialized();
}

cvedix_face_recognition_ort_node::~cvedix_face_recognition_ort_node() {
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Total recognitions: %d",
        node_name.c_str(), total_recognitions.load(std::memory_order_relaxed)));
    deinitialized();
}

void cvedix_face_recognition_ort_node::set_config(const cvedix_face_utils::RecognitionConfig& config) {
    config_ = config;
    
    if (config_.voting_enabled) {
        voting_buffer_ = std::make_unique<cvedix_face_utils::VotingBuffer>(
            config_.voting_window_size,
            config_.voting_majority_threshold
        );
    } else {
        voting_buffer_.reset();
    }
    
    database_.similarity_threshold = config_.similarity_threshold;
    database_.confidence_margin = config_.confidence_margin;
}

void cvedix_face_recognition_ort_node::set_personal_threshold(const std::string& name, float threshold) {
    personal_thresholds_[name] = threshold;
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Set personal threshold for %s: %.2f", node_name.c_str(), name.c_str(), threshold));
}

float cvedix_face_recognition_ort_node::get_personal_threshold(const std::string& name) const {
    auto it = personal_thresholds_.find(name);
    if (it != personal_thresholds_.end()) {
        return it->second;
    }
    return similarity_threshold;
}

std::pair<std::string, float> cvedix_face_recognition_ort_node::get_stable_result(int track_id) {
    if (voting_buffer_) {
        return voting_buffer_->get_stable_result(track_id);
    }
    return {last_match_.name, last_match_.score};
}

bool cvedix_face_recognition_ort_node::load_database(const std::string& path) {
    database_path_ = path;
    database_.similarity_threshold = similarity_threshold;
    database_.confidence_margin = confidence_margin;
    return database_.load(path);
}

void cvedix_face_recognition_ort_node::print_database_stats() {
    database_.print_statistics();
}

// ==================== Direct Processing API ====================

cv::Mat cvedix_face_recognition_ort_node::alignFace(const cv::Mat& src_img, float landmarks[5][2]) {
    return ort_recognizer_->alignCrop(src_img, landmarks);
}

std::vector<float> cvedix_face_recognition_ort_node::extractEmbedding(const cv::Mat& aligned_face) {
    return ort_recognizer_->extractEmbedding(aligned_face);
}

int cvedix_face_recognition_ort_node::getEmbeddingDim() const {
    return embedding_size_;
}

void cvedix_face_recognition_ort_node::prepare(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch,
    std::vector<cv::Mat>& mats_to_infer) {

    assert(frame_meta_with_batch.size() == 1);
    auto& frame_meta = frame_meta_with_batch[0];

    current_tta_info_.clear();
    aligned_faces_.clear();
    face_indices_.clear();

    for (size_t face_idx = 0; face_idx < frame_meta->face_targets.size(); face_idx++) {
        auto& face_target = frame_meta->face_targets[face_idx];
        cv::Mat aligned_face;

        // Face alignment
        if (enable_alignment && face_target->key_points.size() >= 5) {
            float face_keypoints[5][2] = {
                {face_target->key_points[0].first, face_target->key_points[0].second},
                {face_target->key_points[1].first, face_target->key_points[1].second},
                {face_target->key_points[2].first, face_target->key_points[2].second},
                {face_target->key_points[3].first, face_target->key_points[3].second},
                {face_target->key_points[4].first, face_target->key_points[4].second}
            };
            aligned_face = ort_recognizer_->alignCrop(frame_meta->frame, face_keypoints);
        } else {
            cv::Rect face_rect(face_target->x, face_target->y,
                               face_target->width, face_target->height);
            face_rect.x = std::max(0, face_rect.x);
            face_rect.y = std::max(0, face_rect.y);
            face_rect.width = std::min(face_rect.width, frame_meta->frame.cols - face_rect.x);
            face_rect.height = std::min(face_rect.height, frame_meta->frame.rows - face_rect.y);
            if (face_rect.width <= 0 || face_rect.height <= 0) continue;
            aligned_face = frame_meta->frame(face_rect).clone();
            cv::resize(aligned_face, aligned_face, cv::Size(input_width, input_height));
        }

        // TTA: Generate augmented versions
        if (config_.tta.enabled) {
            aligned_faces_.push_back(aligned_face);
            face_indices_.push_back(static_cast<int>(face_idx));
            current_tta_info_.push_back({static_cast<int>(face_idx), 0});

            if (config_.tta.use_flip) {
                cv::Mat flipped;
                cv::flip(aligned_face, flipped, 1);
                aligned_faces_.push_back(flipped);
                face_indices_.push_back(static_cast<int>(face_idx));
                current_tta_info_.push_back({static_cast<int>(face_idx), 1});
            }

            if (config_.tta.use_brightness) {
                cv::Mat bright, dark;
                aligned_face.convertTo(bright, -1, 1.0, 20);
                aligned_face.convertTo(dark, -1, 1.0, -20);
                aligned_faces_.push_back(bright);
                face_indices_.push_back(static_cast<int>(face_idx));
                current_tta_info_.push_back({static_cast<int>(face_idx), 2});
                aligned_faces_.push_back(dark);
                face_indices_.push_back(static_cast<int>(face_idx));
                current_tta_info_.push_back({static_cast<int>(face_idx), 3});
            }
        } else {
            aligned_faces_.push_back(aligned_face);
            face_indices_.push_back(static_cast<int>(face_idx));
            current_tta_info_.push_back({static_cast<int>(face_idx), 0});
        }
    }

    // Pass aligned faces for inference (not used by ONNX Runtime path, but required by base class)
    mats_to_infer = aligned_faces_;
}

void cvedix_face_recognition_ort_node::preprocess(
    const std::vector<cv::Mat>& mats_to_infer,
    cv::Mat& blob_to_infer) {
    // ORT handles preprocessing internally in extractEmbedding()
    // This is just to satisfy the base class interface
    if (!mats_to_infer.empty()) {
        blob_to_infer = cv::Mat();  // Empty blob, not used
    }
}

void cvedix_face_recognition_ort_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (aligned_faces_.empty() || frame_meta_with_batch.empty()) return;

    auto& frame_meta = frame_meta_with_batch[0];

    // Group embeddings by face index (for TTA averaging)
    std::map<int, std::vector<std::vector<float>>> face_embeddings;

    for (size_t i = 0; i < aligned_faces_.size() && i < current_tta_info_.size(); i++) {
        // Extract embedding using ORT
        std::vector<float> embedding = ort_recognizer_->extractEmbedding(aligned_faces_[i]);
        int face_idx = current_tta_info_[i].face_index;
        face_embeddings[face_idx].push_back(embedding);
    }

    // Process each face
    for (auto& [face_idx, embeddings] : face_embeddings) {
        if (face_idx >= static_cast<int>(frame_meta->face_targets.size())) continue;

        // TTA: Average embeddings if multiple
        std::vector<float> final_embedding;
        if (embeddings.size() > 1) {
            final_embedding = cvedix_face_utils::average_embeddings(embeddings);
        } else {
            final_embedding = embeddings[0];
        }

        // Store embedding
        frame_meta->face_targets[face_idx]->embeddings = final_embedding;

        // Match against database
        auto match = database_.find_match(final_embedding);
        
        // ID-Specific Threshold check
        if (config_.id_specific_threshold_enabled && match.confident) {
            float personal_threshold = get_personal_threshold(match.name);
            if (match.score < personal_threshold) {
                match.confident = false;
                match.name = "Unknown";
            }
        }

        last_match_ = match;

        // Voting: Add to buffer and get stable result
        std::string final_name = match.name;
        float final_score = match.score;

        if (config_.voting_enabled && voting_buffer_) {
            int track_id = frame_meta->face_targets[face_idx]->track_id;
            voting_buffer_->add_vote(track_id, match.name, match.score);
            
            auto stable = voting_buffer_->get_stable_result(track_id);
            if (stable.first != "Unknown") {
                final_name = stable.first;
                final_score = stable.second;
            }
        }

        // Update face target
        frame_meta->face_targets[face_idx]->identify = final_name;
        frame_meta->face_targets[face_idx]->identify_score = final_score;

        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Recognition: %s (score=%.3f, voting=%s)",
            node_name.c_str(), final_name.c_str(), final_score,
            config_.voting_enabled ? "ON" : "OFF"));

        total_recognitions.fetch_add(1, std::memory_order_relaxed);
    }
}

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_ORT
