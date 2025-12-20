/**
 * @file cvedix_face_recognition_node.cpp
 * @brief Face recognition node with advanced accuracy improvement techniques
 * 
 * @details
 * This node provides face recognition capabilities using InsightFace/ArcFace models.
 * It includes three optional accuracy improvement techniques:
 * 
 * ## 1. Temporal Voting (Default: ON)
 * Collects recognition results over multiple frames and returns majority vote.
 * Reduces flickering and improves stability in video streams.
 * - Window size: Configurable (default 10 frames)
 * - Majority threshold: Configurable (default 50%)
 * 
 * ## 2. TTA - Test Time Augmentation (Default: OFF)
 * Generates multiple augmented versions of input face:
 * - Original image
 * - Horizontally flipped
 * - Brightness variations (optional)
 * Averages embeddings for more robust matching. Trade-off: ~3x slower.
 * 
 * ## 3. ID-Specific Threshold (Default: OFF)
 * Allows setting different similarity thresholds per person.
 * - Higher threshold for common-looking faces
 * - Lower threshold for distinctive faces
 * 
 * ## Pipeline Integration
 * ```
 * Source → Face Detector → Face Recognition → OSD → Display
 * ```
 * 
 * ## Input Requirements
 * - Face targets with 5-point landmarks (from YuNet, RetinaFace, etc.)
 * - Or bounding boxes (alignment will be skipped)
 * 
 * ## Output
 * - face_targets[i]->identify: Recognized name or "Unknown"
 * - face_targets[i]->identify_score: Similarity score
 * - face_targets[i]->embeddings: 512-dim feature vector
 * 
 * ## Configuration Presets
 * - RecognitionConfig::fast() - Real-time priority
 * - RecognitionConfig::balanced() - Default (voting only)
 * - RecognitionConfig::high_accuracy() - All techniques enabled
 * 
 * @author CVEDIX Team
 * @date 2024
 * @copyright CVEDIX Corporation
 * 
 * @see cvedix_face_registration_node.h For face registration with augmentation
 * @see face_recognition_utils.h For VotingBuffer, TTAConfig, RecognitionConfig
 */

#include "cvedix_face_recognition_node.h"
#include "backends/opencv_dnn_backend.h"
#ifdef CVEDIX_WITH_ORT
#include "backends/onnx_runtime_backend.h"
#endif
#ifdef CVEDIX_WITH_TRT
#include "backends/tensorrt_backend.h"
#endif
#ifdef CVEDIX_WITH_LICENSE
#include "cvedix/utils/license/cvedix_license_manager.h"
#endif
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

namespace cvedix_nodes {

// Constants
static constexpr float EMBEDDING_NORM_EPSILON = 1e-6f;  ///< Epsilon for L2 normalization

/**
 * @brief Initialize the appropriate backend based on selection
 * 
 * Priority when AUTO: TensorRT > ONNX Runtime > OpenCV DNN
 */
void cvedix_face_recognition_node::initializeBackend(
    const std::string& model_path, 
    FaceRecognitionBackend backend) {
    
    // Auto-detect best available backend
    if (backend == FaceRecognitionBackend::AUTO) {
        backend = detectBestBackend();
    }
    
    // Validate backend availability
    if (!isBackendAvailable(backend)) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Requested backend %s not available, falling back to OPENCV_DNN",
            node_name.c_str(), backendToString(backend).c_str()));
        backend = FaceRecognitionBackend::OPENCV_DNN;
    }
    
    selected_backend_ = backend;
    
    // Create backend
    switch (backend) {
    #ifdef CVEDIX_WITH_TRT
        case FaceRecognitionBackend::TENSORRT:
            backend_ = std::make_unique<TensorRTBackend>(model_path, input_width, input_height);
            break;
    #endif
    #ifdef CVEDIX_WITH_ORT
        case FaceRecognitionBackend::ONNX_RUNTIME:
            backend_ = std::make_unique<OnnxRuntimeBackend>(model_path, input_width, input_height);
            break;
    #endif
        case FaceRecognitionBackend::OPENCV_DNN:
        default:
            backend_ = std::make_unique<OpenCVDnnBackend>(model_path, input_width, input_height);
            break;
    }
    
    if (backend_ && backend_->isReady()) {
        embedding_size_ = backend_->getEmbeddingDim();
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Initialized backend: %s (emb_dim=%d)",
            node_name.c_str(), backend_->getBackendName().c_str(), embedding_size_));
    } else {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Failed to initialize backend: %s",
            node_name.c_str(), backendToString(backend).c_str()));
    }
}

/**
 * @brief Default constructor with balanced mode configuration
 * 
 * Creates a face recognition node with default balanced settings:
 * - Temporal voting: ON (window=10, threshold=50%)
 * - TTA: OFF
 * - ID-specific threshold: OFF
 * 
 * @param node_name Unique name for the node
 * @param model_path Path to ONNX face recognition model (InsightFace/ArcFace)
 * @param database_path Path to registered faces database (empty = no database)
 * @param backend Backend selection (AUTO = choose best available)
 * @param input_width Model input width (default 112 for InsightFace)
 * @param input_height Model input height (default 112 for InsightFace)
 * @param enable_alignment Enable 5-point landmark alignment (recommended)
 */
cvedix_face_recognition_node::cvedix_face_recognition_node(
    std::string node_name,
    std::string model_path,
    std::string database_path,
    FaceRecognitionBackend backend,
    int input_width,
    int input_height,
    bool enable_alignment)
    : cvedix_face_recognition_node(
        node_name, model_path, database_path,
        cvedix_face_utils::RecognitionConfig::balanced(),  // Default: voting ON, TTA OFF
        backend,
        input_width, input_height) {
    this->enable_alignment = enable_alignment;
}

/**
 * @brief Constructor with custom recognition configuration
 * 
 * Creates a face recognition node with user-specified configuration.
 * Use RecognitionConfig presets or customize individual settings:
 * - RecognitionConfig::fast() for real-time applications
 * - RecognitionConfig::balanced() for general use
 * - RecognitionConfig::high_accuracy() for verification scenarios
 * 
 * @param node_name Unique name for the node
 * @param model_path Path to ONNX face recognition model
 * @param database_path Path to registered faces database
 * @param config Recognition configuration (voting, TTA, thresholds)
 * @param backend Backend selection (AUTO = choose best available)
 * @param input_width Model input width
 * @param input_height Model input height
 */
cvedix_face_recognition_node::cvedix_face_recognition_node(
    std::string node_name,
    std::string model_path,
    std::string database_path,
    const cvedix_face_utils::RecognitionConfig& config,
    FaceRecognitionBackend backend,
    int input_width,
    int input_height)
    : cvedix_secondary_infer_node(node_name, model_path, "", "",
                                   input_width, input_height,
                                   1, std::vector<int>(), 0, 0,
                                   0, 1.0f / 128.0f,
                                   cv::Scalar(127.5f, 127.5f, 127.5f),
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

    // Apply config to database
    similarity_threshold = config_.similarity_threshold;
    confidence_margin = config_.confidence_margin;

    // Initialize backend (auto-detects best available if AUTO)
    initializeBackend(model_path, backend);

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Loaded model: %s (backend=%s, emb=%d, voting=%s, tta=%s)",
        node_name.c_str(), model_path.c_str(),
        get_backend_name().c_str(), embedding_size_,
        config_.voting_enabled ? "ON" : "OFF",
        config_.tta.enabled ? "ON" : "OFF"));

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

cvedix_face_recognition_node::~cvedix_face_recognition_node() {
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Total recognitions: %d",
        node_name.c_str(), total_recognitions.load(std::memory_order_relaxed)));
    deinitialized();
}

/**
 * @brief Update recognition configuration at runtime
 * 
 * Allows changing voting, TTA, and threshold settings after construction.
 * Voting buffer is automatically recreated if voting settings change.
 * 
 * @param config New recognition configuration
 */
void cvedix_face_recognition_node::set_config(const cvedix_face_utils::RecognitionConfig& config) {
    config_ = config;
    
    // Re-initialize voting buffer
    if (config_.voting_enabled) {
        voting_buffer_ = std::make_unique<cvedix_face_utils::VotingBuffer>(
            config_.voting_window_size,
            config_.voting_majority_threshold
        );
    } else {
        voting_buffer_.reset();
    }
    
    // Update database thresholds
    database_.similarity_threshold = config_.similarity_threshold;
    database_.confidence_margin = config_.confidence_margin;
}

/**
 * @brief Set individual similarity threshold for a specific person
 * 
 * ID-Specific Threshold technique: Allows different security levels per person.
 * - Use higher threshold (e.g., 0.8) for people with common facial features
 * - Use lower threshold (e.g., 0.5) for people with distinctive features
 * 
 * @param name Person name (must match registered name in database)
 * @param threshold Similarity threshold (0.0 - 1.0)
 */
void cvedix_face_recognition_node::set_personal_threshold(const std::string& name, float threshold) {
    personal_thresholds_[name] = threshold;
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Set personal threshold for %s: %.2f", node_name.c_str(), name.c_str(), threshold));
}

float cvedix_face_recognition_node::get_personal_threshold(const std::string& name) const {
    auto it = personal_thresholds_.find(name);
    if (it != personal_thresholds_.end()) {
        return it->second;
    }
    return similarity_threshold;  // Global threshold as default
}

/**
 * @brief Get stable recognition result from voting buffer
 * 
 * Returns the majority-voted result for a face track.
 * Only meaningful when voting is enabled.
 * 
 * @param track_id Face track ID from detector/tracker
 * @return Pair of (name, score) - stable result or ("Unknown", 0.0)
 */
std::pair<std::string, float> cvedix_face_recognition_node::get_stable_result(int track_id) {
    if (voting_buffer_) {
        return voting_buffer_->get_stable_result(track_id);
    }
    return {last_match_.name, last_match_.score};
}

bool cvedix_face_recognition_node::load_database(const std::string& path) {
    database_path_ = path;
    database_.similarity_threshold = similarity_threshold;
    database_.confidence_margin = confidence_margin;
    return database_.load(path);
}

void cvedix_face_recognition_node::print_database_stats() {
    database_.print_statistics();
}

/**
 * @brief Prepare face images for inference
 * 
 * Handles face alignment and TTA augmentation:
 * 1. If landmarks available && alignment enabled: Apply similarity transform
 * 2. Otherwise: Simple crop and resize
 * 3. If TTA enabled: Generate flipped/brightness variants
 * 
 * @param frame_meta_with_batch Input frame with detected faces
 * @param mats_to_infer Output: Aligned face images ready for inference
 */
void cvedix_face_recognition_node::prepare(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch,
    std::vector<cv::Mat>& mats_to_infer) {

    assert(frame_meta_with_batch.size() == 1);
    auto& frame_meta = frame_meta_with_batch[0];

    current_tta_info_.clear();

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
            alignCrop(frame_meta->frame, face_keypoints, aligned_face);
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
            // Original
            mats_to_infer.push_back(aligned_face);
            current_tta_info_.push_back({static_cast<int>(face_idx), 0});

            // Flipped
            if (config_.tta.use_flip) {
                cv::Mat flipped;
                cv::flip(aligned_face, flipped, 1);  // Horizontal flip
                mats_to_infer.push_back(flipped);
                current_tta_info_.push_back({static_cast<int>(face_idx), 1});
            }

            // Brightness variations (optional)
            if (config_.tta.use_brightness) {
                cv::Mat bright, dark;
                aligned_face.convertTo(bright, -1, 1.0, 20);  // +brightness
                aligned_face.convertTo(dark, -1, 1.0, -20);   // -brightness
                mats_to_infer.push_back(bright);
                current_tta_info_.push_back({static_cast<int>(face_idx), 2});
                mats_to_infer.push_back(dark);
                current_tta_info_.push_back({static_cast<int>(face_idx), 3});
            }
        } else {
            // No TTA - just original
            mats_to_infer.push_back(aligned_face);
            current_tta_info_.push_back({static_cast<int>(face_idx), 0});
        }
    }
}

/**
 * @brief Preprocess face images with InsightFace normalization
 * 
 * Applies standard InsightFace preprocessing:
 * - Normalization: (pixel - 127.5) / 128.0
 * - Color conversion: BGR → RGB
 * - Output format: NCHW float32 blob
 * 
 * @param mats_to_infer Input aligned face images
 * @param blob_to_infer Output blob ready for DNN inference
 */
void cvedix_face_recognition_node::preprocess(
    const std::vector<cv::Mat>& mats_to_infer,
    cv::Mat& blob_to_infer) {

    if (mats_to_infer.empty()) return;

    cv::dnn::blobFromImages(mats_to_infer, blob_to_infer,
                            1.0f / 128.0f,
                            cv::Size(input_width, input_height),
                            cv::Scalar(127.5f, 127.5f, 127.5f),
                            true, false, CV_32F);
}

/**
 * @brief Postprocess: Extract embeddings, match against database, apply voting
 * 
 * Complete recognition pipeline:
 * 1. Extract embeddings from DNN output
 * 2. L2 normalize embeddings
 * 3. If TTA enabled: Average embeddings from augmented variants
 * 4. Match against database with margin checking
 * 5. If ID-specific threshold enabled: Apply personal thresholds
 * 6. If voting enabled: Add to voting buffer and get stable result
 * 7. Update face_target with recognition result
 * 
 * @param raw_outputs DNN output (embeddings)
 * @param frame_meta_with_batch Frame metadata to update with results
 */
void cvedix_face_recognition_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (raw_outputs.empty() || frame_meta_with_batch.empty()) return;

    auto& frame_meta = frame_meta_with_batch[0];
    const cv::Mat& output = raw_outputs[0];

    int batch_size = (output.dims == 2) ? output.size[0] : 1;
    int emb_dim = (output.dims == 2) ? output.size[1] : output.size[0];

    // Group embeddings by face index (for TTA averaging)
    std::map<int, std::vector<std::vector<float>>> face_embeddings;

    for (int i = 0; i < batch_size && i < static_cast<int>(current_tta_info_.size()); i++) {
        const float* output_ptr = (output.dims == 2) ? output.ptr<float>(i) : output.ptr<float>();

        std::vector<float> embedding(emb_dim);
        std::copy(output_ptr, output_ptr + emb_dim, embedding.begin());

        // L2 normalize
        float norm = 0.0f;
        for (float val : embedding) norm += val * val;
        norm = std::sqrt(norm);
        if (norm > EMBEDDING_NORM_EPSILON) {
            for (float& val : embedding) val /= norm;
        }

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

/**
 * @brief Compute similarity transform matrix for face alignment
 * 
 * Uses 5-point landmarks (2 eyes, nose, 2 mouth corners) to compute
 * an affine transformation that aligns faces to a standard template.
 * 
 * Standard template is from InsightFace (112x112):
 * - Left eye: (38.29, 51.70)
 * - Right eye: (73.53, 51.50)
 * - Nose: (56.03, 71.74)
 * - Left mouth: (41.55, 92.37)
 * - Right mouth: (70.73, 92.20)
 * 
 * @param src Source 5-point landmarks [5][2] = {{x,y}, ...}
 * @return 2x3 affine transformation matrix
 */
cv::Mat cvedix_face_recognition_node::getSimilarityTransformMatrix(float src[5][2]) {
    using namespace cv;

    float dst[5][2] = {
        {38.2946f, 51.6963f}, {73.5318f, 51.5014f}, {56.0252f, 71.7366f},
        {41.5493f, 92.3655f}, {70.7299f, 92.2041f}
    };

    float avg0 = (src[0][0] + src[1][0] + src[2][0] + src[3][0] + src[4][0]) / 5;
    float avg1 = (src[0][1] + src[1][1] + src[2][1] + src[3][1] + src[4][1]) / 5;
    float src_mean[2] = { avg0, avg1 };
    float dst_mean[2] = { 56.0262f, 71.9008f };

    float src_demean[5][2], dst_demean[5][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 5; j++) {
            src_demean[j][i] = src[j][i] - src_mean[i];
            dst_demean[j][i] = dst[j][i] - dst_mean[i];
        }
    }

    double A00 = 0, A01 = 0, A10 = 0, A11 = 0;
    for (int i = 0; i < 5; i++) {
        A00 += dst_demean[i][0] * src_demean[i][0];
        A01 += dst_demean[i][0] * src_demean[i][1];
        A10 += dst_demean[i][1] * src_demean[i][0];
        A11 += dst_demean[i][1] * src_demean[i][1];
    }
    A00 /= 5; A01 /= 5; A10 /= 5; A11 /= 5;

    Mat A = (Mat_<double>(2, 2) << A00, A01, A10, A11);
    double d[2] = { 1.0, 1.0 };
    if (A00 * A11 - A01 * A10 < 0) d[1] = -1;

    double T[3][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
    Mat s, u, vt;
    SVD::compute(A, s, u, vt);

    double smax = std::max(s.ptr<double>(0)[0], s.ptr<double>(1)[0]);
    double tol = smax * 2 * FLT_MIN;
    int rank = (s.ptr<double>(0)[0] > tol ? 1 : 0) + (s.ptr<double>(1)[0] > tol ? 1 : 0);

    double det_u = u.ptr<double>(0)[0] * u.ptr<double>(1)[1] - u.ptr<double>(0)[1] * u.ptr<double>(1)[0];
    double det_vt = vt.ptr<double>(0)[0] * vt.ptr<double>(1)[1] - vt.ptr<double>(0)[1] * vt.ptr<double>(1)[0];

    Mat D = (Mat_<double>(2, 2) << d[0], 0.0, 0.0, d[1]);
    Mat uDvt = (rank == 1 && det_u * det_vt > 0) ? u * vt : u * D * vt;
    T[0][0] = uDvt.ptr<double>(0)[0]; T[0][1] = uDvt.ptr<double>(0)[1];
    T[1][0] = uDvt.ptr<double>(1)[0]; T[1][1] = uDvt.ptr<double>(1)[1];

    double var = 0;
    for (int i = 0; i < 5; i++) {
        var += src_demean[i][0] * src_demean[i][0] + src_demean[i][1] * src_demean[i][1];
    }
    var /= 5;

    double scale = (1.0 / var) * (s.ptr<double>(0)[0] * d[0] + s.ptr<double>(1)[0] * d[1]);
    T[0][2] = dst_mean[0] - scale * (T[0][0] * src_mean[0] + T[0][1] * src_mean[1]);
    T[1][2] = dst_mean[1] - scale * (T[1][0] * src_mean[0] + T[1][1] * src_mean[1]);
    T[0][0] *= scale; T[0][1] *= scale; T[1][0] *= scale; T[1][1] *= scale;

    return (Mat_<double>(2, 3) << T[0][0], T[0][1], T[0][2], T[1][0], T[1][1], T[1][2]);
}

/**
 * @brief Align and crop face using 5-point landmarks
 * 
 * Applies similarity transform to align face to standard template,
 * then crops to model input size (default 112x112).
 * 
 * @param src_img Source image containing the face
 * @param src_point 5-point landmarks [5][2] from face detector
 * @param aligned_img Output: Aligned and cropped face
 */
void cvedix_face_recognition_node::alignCrop(cv::Mat& src_img, float src_point[5][2], cv::Mat& aligned_img) {
    cv::warpAffine(src_img, aligned_img, getSimilarityTransformMatrix(src_point),
                   cv::Size(input_width, input_height), cv::INTER_LINEAR);
}

} // namespace cvedix_nodes
