#include "cvedix_face_registration_node.h"
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

namespace cvedix_nodes {

// Constants
static constexpr float EMBEDDING_NORM_EPSILON = 1e-6f;

cvedix_face_registration_node::cvedix_face_registration_node(
    std::string node_name,
    std::string model_path,
    std::string database_path,
    bool enable_augmentation,
    std::string glasses_template,
    std::string mask_template,
    std::string hat_template,
    int input_width,
    int input_height)
    : cvedix_secondary_infer_node(node_name, model_path, "", "",
                                   input_width, input_height,
                                   1, std::vector<int>(), 0, 0,
                                   0,            // crop_padding
                                   1.0f / 128.0f,  // scale for InsightFace
                                   cv::Scalar(127.5f, 127.5f, 127.5f),  // mean
                                   cv::Scalar(1, 1, 1),  // std
                                   true,   // swap_rb
                                   false), // swap_chn
      enable_augmentation(enable_augmentation),
      database_path_(database_path) {

    // Load ONNX model
    try {
        net = cv::dnn::readNetFromONNX(model_path);
        if (net.empty()) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Failed to load ONNX model: %s", node_name.c_str(), model_path.c_str()));
            assert(false);
        }

        // Set backend and target
        #ifdef CVEDIX_WITH_CUDA
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        CVEDIX_INFO(cvedix_utils::string_format("[%s] Using CUDA backend", node_name.c_str()));
        #else
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        CVEDIX_INFO(cvedix_utils::string_format("[%s] Using CPU backend", node_name.c_str()));
        #endif

        // Detect embedding size
        std::vector<std::string> output_names = net.getUnconnectedOutLayersNames();
        if (!output_names.empty()) {
            try {
                std::vector<cv::Mat> output_blobs;
                cv::Mat dummy = cv::Mat::zeros(input_height, input_width, CV_8UC3);
                cv::Mat dummy_blob;
                cv::dnn::blobFromImage(dummy, dummy_blob, 1.0f / 128.0f,
                                        cv::Size(), cv::Scalar(127.5f, 127.5f, 127.5f),
                                        false, false, CV_32F);
                net.setInput(dummy_blob);
                net.forward(output_blobs, output_names);

                if (!output_blobs.empty() && output_blobs[0].dims >= 2) {
                    embedding_size_ = output_blobs[0].size[output_blobs[0].dims - 1];
                    CVEDIX_INFO(cvedix_utils::string_format(
                        "[%s] Detected embedding size: %d", node_name.c_str(), embedding_size_));
                }
            } catch (const std::exception& e) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Could not detect embedding size, using default 512: %s",
                    node_name.c_str(), e.what()));
            }
        }

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Loaded face recognition model: %s (embedding=%d)",
            node_name.c_str(), model_path.c_str(), embedding_size_));

    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Exception loading model: %s", node_name.c_str(), e.what()));
        assert(false);
    }

    // Initialize augmenter if enabled
    if (enable_augmentation) {
        augmenter_ = std::make_unique<cvedix_face_utils::FaceAugmenter>(
            glasses_template, mask_template, hat_template, input_width);
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Augmentation enabled: %d variants per face",
            node_name.c_str(), augmenter_->augmentation_count()));
    }

    // Load existing database (for appending)
    if (!database_path.empty()) {
        if (load_database()) {
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Loaded existing database: %s", node_name.c_str(), database_path.c_str()));
        }
    }

    this->initialized();
}

cvedix_face_registration_node::~cvedix_face_registration_node() {
    // Auto-save database on destruction
    if (!database_path_.empty()) {
        save_database();
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Total registrations: %d",
        node_name.c_str(),
        total_registrations.load(std::memory_order_relaxed)));

    deinitialized();
}

void cvedix_face_registration_node::set_registration_name(const std::string& person_name) {
    pending_registration_name_ = person_name;
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Registration pending for: %s", node_name.c_str(), person_name.c_str()));
}

bool cvedix_face_registration_node::save_database() {
    return database_.save(database_path_);
}

bool cvedix_face_registration_node::load_database() {
    return database_.load(database_path_);
}

void cvedix_face_registration_node::clear_database() {
    database_.clear();
    CVEDIX_INFO(cvedix_utils::string_format("[%s] Database cleared", node_name.c_str()));
}

void cvedix_face_registration_node::print_database_stats() {
    database_.print_statistics();
}

void cvedix_face_registration_node::prepare(
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch,
    std::vector<cv::Mat>& mats_to_infer) {

    assert(frame_meta_with_batch.size() == 1);
    auto& frame_meta = frame_meta_with_batch[0];

    current_augmentation_info_.clear();

    for (size_t face_idx = 0; face_idx < frame_meta->face_targets.size(); face_idx++) {
        auto& face_target = frame_meta->face_targets[face_idx];
        cv::Mat aligned_face;

        // Perform face alignment if landmarks available
        if (face_target->key_points.size() >= 5) {
            float face_keypoints[5][2] = {
                {face_target->key_points[0].first, face_target->key_points[0].second},
                {face_target->key_points[1].first, face_target->key_points[1].second},
                {face_target->key_points[2].first, face_target->key_points[2].second},
                {face_target->key_points[3].first, face_target->key_points[3].second},
                {face_target->key_points[4].first, face_target->key_points[4].second}
            };
            alignCrop(frame_meta->frame, face_keypoints, aligned_face);
        } else {
            // Simple crop
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

        // Generate augmented variants if enabled
        if (enable_augmentation && augmenter_) {
            auto variants = augmenter_->generate_all_variants(aligned_face);
            for (const auto& variant : variants) {
                mats_to_infer.push_back(variant.image);
                current_augmentation_info_.push_back({
                    static_cast<int>(face_idx),
                    variant.type
                });
            }
        } else {
            // Just use original
            mats_to_infer.push_back(aligned_face);
            current_augmentation_info_.push_back({
                static_cast<int>(face_idx),
                cvedix_face_utils::AugmentationType::ORIGINAL
            });
        }
    }
}

void cvedix_face_registration_node::preprocess(
    const std::vector<cv::Mat>& mats_to_infer,
    cv::Mat& blob_to_infer) {

    if (mats_to_infer.empty()) return;

    // InsightFace preprocessing: (pixel - 127.5) / 128.0, BGR->RGB
    cv::dnn::blobFromImages(mats_to_infer, blob_to_infer,
                            1.0f / 128.0f,
                            cv::Size(input_width, input_height),
                            cv::Scalar(127.5f, 127.5f, 127.5f),
                            true,   // swapRB: BGR->RGB
                            false,  // crop
                            CV_32F);
}

void cvedix_face_registration_node::postprocess(
    const std::vector<cv::Mat>& raw_outputs,
    const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

    if (raw_outputs.empty() || frame_meta_with_batch.empty()) return;

    auto& frame_meta = frame_meta_with_batch[0];
    const cv::Mat& output = raw_outputs[0];

    int batch_size = 1;
    int emb_dim = 0;

    if (output.dims == 2) {
        batch_size = output.size[0];
        emb_dim = output.size[1];
    } else if (output.dims == 1) {
        batch_size = 1;
        emb_dim = output.size[0];
    } else {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Unexpected output shape", node_name.c_str()));
        return;
    }

    // Extract and store embeddings
    last_registration_count_ = 0;

    for (int i = 0; i < batch_size && i < static_cast<int>(current_augmentation_info_.size()); i++) {
        const float* output_ptr = output.dims == 2 ?
                                   output.ptr<float>(i) :
                                   output.ptr<float>();

        // Extract embedding
        std::vector<float> embedding(emb_dim);
        std::copy(output_ptr, output_ptr + emb_dim, embedding.begin());

        // L2 normalize
        float norm = 0.0f;
        for (float val : embedding) norm += val * val;
        norm = std::sqrt(norm);
        if (norm > EMBEDDING_NORM_EPSILON) {
            for (float& val : embedding) val /= norm;
        }

        int face_idx = current_augmentation_info_[i].face_index;
        auto aug_type = current_augmentation_info_[i].type;

        // Register to database
        if (!pending_registration_name_.empty()) {
            // Store with base name (all variants under same person)
            database_.add_face(pending_registration_name_, embedding);
            last_registration_count_++;

            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Registered embedding: %s (variant=%d)",
                node_name.c_str(), pending_registration_name_.c_str(),
                static_cast<int>(aug_type)));
        }

        // Also store embedding in face_target
        if (face_idx < static_cast<int>(frame_meta->face_targets.size())) {
            frame_meta->face_targets[face_idx]->embeddings = embedding;
        }
    }

    // Clear pending registration after processing
    if (last_registration_count_ > 0) {
        total_registrations.fetch_add(1, std::memory_order_relaxed);
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Registered %s with %d embedding variants",
            node_name.c_str(), pending_registration_name_.c_str(), last_registration_count_));
        pending_registration_name_.clear();
    }
}

// Face alignment implementation
cv::Mat cvedix_face_registration_node::getSimilarityTransformMatrix(float src[5][2]) {
    using namespace cv;

    // Standard landmarks for 112x112 aligned face
    float dst[5][2] = {
        {38.2946f, 51.6963f},
        {73.5318f, 51.5014f},
        {56.0252f, 71.7366f},
        {41.5493f, 92.3655f},
        {70.7299f, 92.2041f}
    };

    float avg0 = (src[0][0] + src[1][0] + src[2][0] + src[3][0] + src[4][0]) / 5;
    float avg1 = (src[0][1] + src[1][1] + src[2][1] + src[3][1] + src[4][1]) / 5;

    float src_mean[2] = { avg0, avg1 };
    float dst_mean[2] = { 56.0262f, 71.9008f };

    float src_demean[5][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 5; j++) {
            src_demean[j][i] = src[j][i] - src_mean[i];
        }
    }
    float dst_demean[5][2];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 5; j++) {
            dst_demean[j][i] = dst[j][i] - dst_mean[i];
        }
    }

    double A00 = 0.0, A01 = 0.0, A10 = 0.0, A11 = 0.0;
    for (int i = 0; i < 5; i++) {
        A00 += dst_demean[i][0] * src_demean[i][0];
        A01 += dst_demean[i][0] * src_demean[i][1];
        A10 += dst_demean[i][1] * src_demean[i][0];
        A11 += dst_demean[i][1] * src_demean[i][1];
    }
    A00 /= 5; A01 /= 5; A10 /= 5; A11 /= 5;

    Mat A = (Mat_<double>(2, 2) << A00, A01, A10, A11);
    double d[2] = { 1.0, 1.0 };
    double detA = A00 * A11 - A01 * A10;
    if (detA < 0) d[1] = -1;

    double T[3][3] = { {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0} };
    Mat s, u, vt;
    SVD::compute(A, s, u, vt);

    double smax = std::max(s.ptr<double>(0)[0], s.ptr<double>(1)[0]);
    double tol = smax * 2 * FLT_MIN;
    int rank = 0;
    if (s.ptr<double>(0)[0] > tol) rank++;
    if (s.ptr<double>(1)[0] > tol) rank++;

    double arr_u[2][2] = {
        {u.ptr<double>(0)[0], u.ptr<double>(0)[1]},
        {u.ptr<double>(1)[0], u.ptr<double>(1)[1]}
    };
    double arr_vt[2][2] = {
        {vt.ptr<double>(0)[0], vt.ptr<double>(0)[1]},
        {vt.ptr<double>(1)[0], vt.ptr<double>(1)[1]}
    };
    double det_u = arr_u[0][0] * arr_u[1][1] - arr_u[0][1] * arr_u[1][0];
    double det_vt = arr_vt[0][0] * arr_vt[1][1] - arr_vt[0][1] * arr_vt[1][0];

    if (rank == 1) {
        if ((det_u * det_vt) > 0) {
            Mat uvt = u * vt;
            T[0][0] = uvt.ptr<double>(0)[0];
            T[0][1] = uvt.ptr<double>(0)[1];
            T[1][0] = uvt.ptr<double>(1)[0];
            T[1][1] = uvt.ptr<double>(1)[1];
        } else {
            double temp = d[1];
            d[1] = -1;
            Mat D = (Mat_<double>(2, 2) << d[0], 0.0, 0.0, d[1]);
            Mat uDvt = u * D * vt;
            T[0][0] = uDvt.ptr<double>(0)[0];
            T[0][1] = uDvt.ptr<double>(0)[1];
            T[1][0] = uDvt.ptr<double>(1)[0];
            T[1][1] = uDvt.ptr<double>(1)[1];
            d[1] = temp;
        }
    } else {
        Mat D = (Mat_<double>(2, 2) << d[0], 0.0, 0.0, d[1]);
        Mat uDvt = u * D * vt;
        T[0][0] = uDvt.ptr<double>(0)[0];
        T[0][1] = uDvt.ptr<double>(0)[1];
        T[1][0] = uDvt.ptr<double>(1)[0];
        T[1][1] = uDvt.ptr<double>(1)[1];
    }

    double var1 = 0.0, var2 = 0.0;
    for (int i = 0; i < 5; i++) {
        var1 += src_demean[i][0] * src_demean[i][0];
        var2 += src_demean[i][1] * src_demean[i][1];
    }
    var1 /= 5; var2 /= 5;

    double scale = 1.0 / (var1 + var2) * (s.ptr<double>(0)[0] * d[0] + s.ptr<double>(1)[0] * d[1]);
    double TS[2];
    TS[0] = T[0][0] * src_mean[0] + T[0][1] * src_mean[1];
    TS[1] = T[1][0] * src_mean[0] + T[1][1] * src_mean[1];
    T[0][2] = dst_mean[0] - scale * TS[0];
    T[1][2] = dst_mean[1] - scale * TS[1];
    T[0][0] *= scale;
    T[0][1] *= scale;
    T[1][0] *= scale;
    T[1][1] *= scale;

    Mat transform_mat = (Mat_<double>(2, 3) << T[0][0], T[0][1], T[0][2], T[1][0], T[1][1], T[1][2]);
    return transform_mat;
}

void cvedix_face_registration_node::alignCrop(
    cv::Mat& src_img,
    float src_point[5][2],
    cv::Mat& aligned_img) {
    cv::Mat warp_mat = getSimilarityTransformMatrix(src_point);
    cv::warpAffine(src_img, aligned_img, warp_mat,
                   cv::Size(input_width, input_height), cv::INTER_LINEAR);
}

} // namespace cvedix_nodes
