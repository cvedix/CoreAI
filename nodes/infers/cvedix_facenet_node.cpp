#include "cvedix_facenet_node.h"
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>

namespace cvedix_nodes {
    
    // ============================================================================
    // FaceNet Recognition Node Implementation
    // ============================================================================
    
    cvedix_facenet_node::cvedix_facenet_node(
        std::string node_name, 
        std::string model_path,
        int input_width,
        int input_height,
        bool enable_alignment,
        std::string pretrained_dataset):
        cvedix_secondary_infer_node(node_name, model_path, "", "", 
                                   input_width, input_height, 
                                   1, std::vector<int>(), 0, 0,
                                   0,  // crop_padding (handled in prepare)
                                   1.0f / 255.0f,  // scale: first divide by 255
                                   cv::Scalar(0.5f, 0.5f, 0.5f),  // mean: then subtract 0.5
                                   cv::Scalar(0.5f, 0.5f, 0.5f),  // std: then divide by 0.5 -> (pixel/255 - 0.5)/0.5
                                   true,  // swap_rb: BGR to RGB
                                   false),  // swap_chn: keep NCHW
        enable_alignment(enable_alignment),
        pretrained_dataset(pretrained_dataset),
        embedding_size(512) {  // FaceNet standard embedding size
        
        // Load ONNX model using OpenCV DNN
        try {
            net = cv::dnn::readNetFromONNX(model_path);
            if (net.empty()) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to load FaceNet ONNX model: %s", 
                    node_name.c_str(), model_path.c_str()));
                assert(false);
            }
            
            // Set backend and target (prefer CUDA if available)
            #ifdef CVEDIX_WITH_CUDA
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Using CUDA backend", node_name.c_str()));
            #else
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Using CPU backend", node_name.c_str()));
            #endif
            
            // Detect embedding size from model output shape
            std::vector<std::string> output_names = net.getUnconnectedOutLayersNames();
            if (!output_names.empty()) {
                try {
                    std::vector<cv::Mat> output_blobs;
                    // Create a dummy input blob [1, 3, height, width] in NCHW format
                    cv::Mat dummy_image = cv::Mat::zeros(input_height, input_width, CV_8UC3);
                    cv::Mat dummy_blob;
                    // FaceNet normalization: (pixel / 255.0 - 0.5) / 0.5
                    cv::dnn::blobFromImage(dummy_image, dummy_blob, 1.0f / 255.0f,
                                          cv::Size(), cv::Scalar(0.5f, 0.5f, 0.5f),
                                          false, false, CV_32F);
                    // Apply std normalization manually
                    dummy_blob /= 0.5f;
                    
                    net.setInput(dummy_blob);
                    net.forward(output_blobs, output_names);
                    
                    if (!output_blobs.empty() && output_blobs[0].dims >= 2) {
                        // Output shape is typically [batch, embedding_dim] or [1, embedding_dim]
                        embedding_size = output_blobs[0].size[output_blobs[0].dims - 1];
                        CVEDIX_INFO(cvedix_utils::string_format("[%s] Detected embedding size: %d", 
                            node_name.c_str(), embedding_size));
                    }
                } catch (const std::exception& e) {
                    CVEDIX_WARN(cvedix_utils::string_format("[%s] Could not detect embedding size, using default 512: %s", 
                        node_name.c_str(), e.what()));
                    embedding_size = 512;
                }
            }
            
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Loaded FaceNet ONNX model: %s\n"
                "  - Pretrained on: %s\n"
                "  - Input size: %dx%d\n"
                "  - Embedding size: %d\n"
                "  - Face alignment: %s", 
                node_name.c_str(), model_path.c_str(),
                pretrained_dataset.c_str(),
                input_width, input_height,
                embedding_size,
                enable_alignment ? "enabled" : "disabled"));
        }
        catch(const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Exception loading FaceNet ONNX model: %s", 
                node_name.c_str(), e.what()));
            assert(false);
        }
        
        this->initialized();
    }
    
    cvedix_facenet_node::~cvedix_facenet_node() {
        CVEDIX_INFO(cvedix_utils::string_format("[%s] Total faces processed: %d", 
            node_name.c_str(), total_faces_processed.load(std::memory_order_relaxed)));
        deinitialized();
    }

    void cvedix_facenet_node::prepare(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, 
        std::vector<cv::Mat>& mats_to_infer) {
        
        // Only one frame at a time for secondary infer node
        assert(frame_meta_with_batch.size() == 1);

        auto& frame_meta = frame_meta_with_batch[0];

        // Process each face target
        for (auto& face_target : frame_meta->face_targets) {
            cv::Mat face_img;
            
            if (enable_alignment && face_target->key_points.size() >= 5) {
                // Perform face alignment using 5-point landmarks
                float face_keypoints[5][2] = {
                    {face_target->key_points[0].first, face_target->key_points[0].second}, // left eye
                    {face_target->key_points[1].first, face_target->key_points[1].second}, // right eye
                    {face_target->key_points[2].first, face_target->key_points[2].second}, // nose
                    {face_target->key_points[3].first, face_target->key_points[3].second}, // left mouth
                    {face_target->key_points[4].first, face_target->key_points[4].second}  // right mouth
                };
                alignCrop(frame_meta->frame, face_keypoints, face_img);
                
                CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Face aligned using 5-point landmarks", 
                    node_name.c_str()));
            } else {
                // Simple crop without alignment
                cv::Rect face_rect(face_target->x, face_target->y, 
                                  face_target->width, face_target->height);
                
                // Add padding if needed
                if (crop_padding != 0) {
                    face_rect.x = std::max(0, face_rect.x - crop_padding);
                    face_rect.y = std::max(0, face_rect.y - crop_padding);
                    face_rect.width = std::min(face_rect.width + 2 * crop_padding, 
                                              frame_meta->frame.cols - face_rect.x);
                    face_rect.height = std::min(face_rect.height + 2 * crop_padding, 
                                               frame_meta->frame.rows - face_rect.y);
                }
                
                // Ensure valid rectangle
                face_rect.x = std::max(0, face_rect.x);
                face_rect.y = std::max(0, face_rect.y);
                face_rect.width = std::min(face_rect.width, frame_meta->frame.cols - face_rect.x);
                face_rect.height = std::min(face_rect.height, frame_meta->frame.rows - face_rect.y);
                
                if (face_rect.width <= 0 || face_rect.height <= 0) {
                    CVEDIX_WARN(cvedix_utils::string_format("[%s] Invalid face rectangle, skipping", 
                        node_name.c_str()));
                    continue;
                }
                
                face_img = frame_meta->frame(face_rect).clone();
                
                // Resize to input size
                if (face_img.rows != input_height || face_img.cols != input_width) {
                    cv::resize(face_img, face_img, cv::Size(input_width, input_height), 
                              0, 0, cv::INTER_LINEAR);
                }
                
                if (enable_alignment) {
                    CVEDIX_DEBUG(cvedix_utils::string_format(
                        "[%s] Face alignment requested but no landmarks available, using simple crop", 
                        node_name.c_str()));
                }
            }
            
            mats_to_infer.push_back(face_img);
        }
        
        total_faces_processed.fetch_add(static_cast<int>(mats_to_infer.size()), std::memory_order_relaxed);
    }

    // Constant for L2 normalization threshold
    static constexpr float EMBEDDING_NORM_EPSILON = 1e-6f;

    void cvedix_facenet_node::preprocess(
        const std::vector<cv::Mat>& mats_to_infer, 
        cv::Mat& blob_to_infer) {
        
        if (mats_to_infer.empty()) {
            return;
        }

        // FaceNet preprocessing: BGR->RGB, normalize (pixel / 255.0 - 0.5) / 0.5
        // This maps pixel values from [0, 255] to [-1, 1]
        // Note: Images are already resized to input_width x input_height in prepare()
        
        // Use blobFromImages with FaceNet normalization
        // - swapRB=true handles BGR->RGB conversion
        // - scale = 1/255 -> pixel / 255
        // - mean = 0.5 -> subtract 0.5
        // - Then divide by 0.5 manually
        // Result: (pixel / 255 - 0.5) / 0.5 = pixel/127.5 - 1.0 (range [-1, 1])
        cv::dnn::blobFromImages(mats_to_infer, blob_to_infer, 
                               1.0f / 255.0f,  // scale
                               cv::Size(input_width, input_height),  // ensure correct size
                               cv::Scalar(0.5f, 0.5f, 0.5f),  // mean
                               true,   // swapRB: BGR->RGB (handles color conversion)
                               false,  // crop
                               CV_32F);
        
        // Apply std normalization (divide by 0.5)
        blob_to_infer /= 0.5f;
    }

    void cvedix_facenet_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        
        if (raw_outputs.empty() || frame_meta_with_batch.empty()) {
            return;
        }

        auto& frame_meta = frame_meta_with_batch[0];
        const cv::Mat& output = raw_outputs[0];  // Get embedding output
        
        // Handle different output shapes
        // Output can be [batch, embedding_dim] (2D) or [embedding_dim] (1D for batch=1)
        int batch_size = 1;
        int emb_dim = 0;
        
        if (output.dims == 2) {
            // Shape: [batch, embedding_dim]
            batch_size = output.size[0];
            emb_dim = output.size[1];
        } else if (output.dims == 1) {
            // Shape: [embedding_dim] (single sample)
            batch_size = 1;
            emb_dim = output.size[0];
        } else if (output.dims == 4 && output.size[2] == 1 && output.size[3] == 1) {
            // Shape: [batch, embedding_dim, 1, 1] (some models output this)
            batch_size = output.size[0];
            emb_dim = output.size[1];
        } else {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Unexpected output shape: %d dimensions", 
                node_name.c_str(), output.dims));
            return;
        }
        
        if (batch_size != static_cast<int>(frame_meta->face_targets.size())) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Batch size mismatch: output=%d, face_targets=%d", 
                node_name.c_str(), batch_size, 
                static_cast<int>(frame_meta->face_targets.size())));
            return;
        }

        // Pre-reshape 4D output outside the loop to avoid repeated reshape operations
        cv::Mat reshaped_output;
        if (output.dims == 4) {
            reshaped_output = output.reshape(1, {batch_size, emb_dim});
        }

        // Extract embeddings for each face
        for (int i = 0; i < batch_size; i++) {
            std::vector<float> embedding(emb_dim);
            
            // Get embedding for this batch item
            const float* output_ptr = nullptr;
            if (output.dims == 2) {
                // 2D: [batch, embedding_dim]
                output_ptr = output.ptr<float>(i);
            } else if (output.dims == 1) {
                // 1D: [embedding_dim] - only one sample
                output_ptr = output.ptr<float>();
            } else if (output.dims == 4) {
                // 4D: [batch, embedding_dim, 1, 1] - use pre-reshaped matrix
                output_ptr = reshaped_output.ptr<float>(i);
            }
            
            std::copy(output_ptr, output_ptr + emb_dim, embedding.begin());

            // L2 normalize (essential for face recognition with cosine similarity)
            float norm = 0.0f;
            for (float val : embedding) {
                norm += val * val;
            }
            norm = std::sqrt(norm);
            
            if (norm > EMBEDDING_NORM_EPSILON) {
                for (float& val : embedding) {
                    val /= norm;
                }
            } else {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Embedding norm too small (%.6f), skipping normalization", 
                    node_name.c_str(), norm));
            }

            // Store embedding in face target
            frame_meta->face_targets[i]->embeddings = embedding;
            
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] Extracted embedding for face %d: dim=%d, norm=%.4f", 
                node_name.c_str(), i, emb_dim, norm));
        }
    }

    // Face alignment implementation (ported from InsightFace node)
    cv::Mat cvedix_facenet_node::getSimilarityTransformMatrix(float src[5][2]) {
        using namespace cv;
        
        // Standard face alignment landmarks for 160x160 output (scaled from 112x112)
        // FaceNet uses 160x160, so we scale the reference landmarks accordingly
        float scale_factor = 160.0f / 112.0f;
        float dst[5][2] = { 
            {38.2946f * scale_factor, 51.6963f * scale_factor},  // left eye
            {73.5318f * scale_factor, 51.5014f * scale_factor},  // right eye
            {56.0252f * scale_factor, 71.7366f * scale_factor},  // nose
            {41.5493f * scale_factor, 92.3655f * scale_factor},  // left mouth
            {70.7299f * scale_factor, 92.2041f * scale_factor}   // right mouth
        };
        
        // Compute mean of src
        float avg0 = 0.0f, avg1 = 0.0f;
        for (int i = 0; i < 5; i++) {
            avg0 += src[i][0];
            avg1 += src[i][1];
        }
        avg0 /= 5.0f;
        avg1 /= 5.0f;
        
        float src_mean[2] = { avg0, avg1 };
        float dst_mean[2] = { 56.0262f * scale_factor, 71.9008f * scale_factor };
        
        // Subtract mean from src and dst
        float src_demean[5][2];
        float dst_demean[5][2];
        for (int i = 0; i < 5; i++) {
            src_demean[i][0] = src[i][0] - src_mean[0];
            src_demean[i][1] = src[i][1] - src_mean[1];
            dst_demean[i][0] = dst[i][0] - dst_mean[0];
            dst_demean[i][1] = dst[i][1] - dst_mean[1];
        }
        
        // Compute correlation matrix
        double A00 = 0.0, A01 = 0.0, A10 = 0.0, A11 = 0.0;
        for (int i = 0; i < 5; i++) {
            A00 += dst_demean[i][0] * src_demean[i][0];
            A01 += dst_demean[i][0] * src_demean[i][1];
            A10 += dst_demean[i][1] * src_demean[i][0];
            A11 += dst_demean[i][1] * src_demean[i][1];
        }
        A00 /= 5.0;
        A01 /= 5.0;
        A10 /= 5.0;
        A11 /= 5.0;
        
        Mat A = (Mat_<double>(2, 2) << A00, A01, A10, A11);
        double d[2] = { 1.0, 1.0 };
        double detA = A00 * A11 - A01 * A10;
        if (detA < 0)
            d[1] = -1;
        
        // SVD decomposition
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
        
        double T[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
        
        if (rank == 1) {
            if ((det_u * det_vt) > 0) {
                Mat uvt = u * vt;
                T[0][0] = uvt.ptr<double>(0)[0];
                T[0][1] = uvt.ptr<double>(0)[1];
                T[1][0] = uvt.ptr<double>(1)[0];
                T[1][1] = uvt.ptr<double>(1)[1];
            } else {
                d[1] = -1;
                Mat D = (Mat_<double>(2, 2) << d[0], 0.0, 0.0, d[1]);
                Mat uDvt = u * D * vt;
                T[0][0] = uDvt.ptr<double>(0)[0];
                T[0][1] = uDvt.ptr<double>(0)[1];
                T[1][0] = uDvt.ptr<double>(1)[0];
                T[1][1] = uDvt.ptr<double>(1)[1];
                d[1] = 1.0;
            }
        } else {
            Mat D = (Mat_<double>(2, 2) << d[0], 0.0, 0.0, d[1]);
            Mat uDvt = u * D * vt;
            T[0][0] = uDvt.ptr<double>(0)[0];
            T[0][1] = uDvt.ptr<double>(0)[1];
            T[1][0] = uDvt.ptr<double>(1)[0];
            T[1][1] = uDvt.ptr<double>(1)[1];
        }
        
        // Compute scale
        double var1 = 0.0, var2 = 0.0;
        for (int i = 0; i < 5; i++) {
            var1 += src_demean[i][0] * src_demean[i][0];
            var2 += src_demean[i][1] * src_demean[i][1];
        }
        var1 /= 5.0;
        var2 /= 5.0;
        
        double scale = 1.0 / (var1 + var2) * (s.ptr<double>(0)[0] * d[0] + s.ptr<double>(1)[0] * d[1]);
        
        // Apply scale and translation
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

    void cvedix_facenet_node::alignCrop(
        cv::Mat& _src_img, 
        float _src_point[5][2], 
        cv::Mat& _aligned_img) {
        cv::Mat warp_mat = getSimilarityTransformMatrix(_src_point);
        cv::warpAffine(_src_img, _aligned_img, warp_mat, 
                      cv::Size(input_width, input_height), cv::INTER_LINEAR);
    }
    
    
    // ============================================================================
    // MTCNN Face Detector Implementation
    // ============================================================================
    
    cvedix_mtcnn_face_detector_node::cvedix_mtcnn_face_detector_node(
        std::string node_name,
        std::string pnet_model,
        std::string rnet_model,
        std::string onet_model,
        float min_face_size,
        std::vector<float> thresholds):
        cvedix_primary_infer_node(node_name, ""),  // Empty model path, we load manually
        min_face_size(min_face_size),
        thresholds(thresholds) {
        
        // Validate thresholds
        if (thresholds.size() != 3) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] MTCNN requires 3 thresholds for [P-Net, R-Net, O-Net], got %d", 
                node_name.c_str(), static_cast<int>(thresholds.size())));
            assert(false);
        }
        
        // Set NMS thresholds for each stage
        nms_thresholds[0] = 0.5f;  // P-Net
        nms_thresholds[1] = 0.7f;  // R-Net
        nms_thresholds[2] = 0.7f;  // O-Net
        
        try {
            // Load the three MTCNN networks
            pnet = cv::dnn::readNetFromONNX(pnet_model);
            rnet = cv::dnn::readNetFromONNX(rnet_model);
            onet = cv::dnn::readNetFromONNX(onet_model);
            
            if (pnet.empty() || rnet.empty() || onet.empty()) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] Failed to load one or more MTCNN networks", node_name.c_str()));
                assert(false);
            }
            
            // Set backend and target
            #ifdef CVEDIX_WITH_CUDA
            pnet.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            pnet.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            rnet.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            rnet.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            onet.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            onet.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Using CUDA backend", node_name.c_str()));
            #else
            pnet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            pnet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            rnet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            rnet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            onet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            onet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Using CPU backend", node_name.c_str()));
            #endif
            
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Loaded MTCNN models\n"
                "  - P-Net: %s\n"
                "  - R-Net: %s\n"
                "  - O-Net: %s\n"
                "  - Min face size: %.1f\n"
                "  - Thresholds: [%.2f, %.2f, %.2f]",
                node_name.c_str(),
                pnet_model.c_str(), rnet_model.c_str(), onet_model.c_str(),
                min_face_size,
                thresholds[0], thresholds[1], thresholds[2]));
        }
        catch(const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception loading MTCNN models: %s", 
                node_name.c_str(), e.what()));
            assert(false);
        }
        
        this->initialized();
    }
    
    cvedix_mtcnn_face_detector_node::~cvedix_mtcnn_face_detector_node() {
        deinitialized();
    }
    
    void cvedix_mtcnn_face_detector_node::run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        
        for (auto& frame_meta : frame_meta_with_batch) {
            if (frame_meta->frame.empty()) {
                continue;
            }
            
            std::vector<FaceBox> detected_faces;
            
            // Stage 1: P-Net (Proposal Network)
            stage1_pnet(frame_meta->frame, detected_faces);
            if (detected_faces.empty()) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] No faces detected in stage 1 (P-Net)", node_name.c_str()));
                continue;
            }
            
            // Stage 2: R-Net (Refine Network)
            stage2_rnet(frame_meta->frame, detected_faces);
            if (detected_faces.empty()) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] No faces detected in stage 2 (R-Net)", node_name.c_str()));
                continue;
            }
            
            // Stage 3: O-Net (Output Network)
            stage3_onet(frame_meta->frame, detected_faces);
            if (detected_faces.empty()) {
                CVEDIX_DEBUG(cvedix_utils::string_format(
                    "[%s] No faces detected in stage 3 (O-Net)", node_name.c_str()));
                continue;
            }
            
            // Convert to face_targets
            for (const auto& face : detected_faces) {
                if (face.score < score_threshold) {
                    continue;
                }
                
                // Convert landmarks from cv::Point2f to std::pair<int, int>
                std::vector<std::pair<int, int>> landmarks;
                for (const auto& pt : face.landmarks) {
                    landmarks.push_back({static_cast<int>(pt.x), static_cast<int>(pt.y)});
                }

                // Create face_target using constructor
                auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                    face.box.x,
                    face.box.y,
                    face.box.width,
                    face.box.height,
                    face.score,
                    landmarks,
                    std::vector<float>()  // Empty embeddings, will be filled by feature encoder
                );
                frame_meta->face_targets.push_back(face_target);
            }
            
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] Detected %d faces (threshold=%.2f)", 
                node_name.c_str(), 
                static_cast<int>(frame_meta->face_targets.size()),
                score_threshold));
        }
    }
    
    void cvedix_mtcnn_face_detector_node::stage1_pnet(
        const cv::Mat& img, 
        std::vector<FaceBox>& boxes) {
        
        // TODO: Implement P-Net stage
        // This is a placeholder - full MTCNN implementation is complex
        // and requires multi-scale image pyramid processing
        
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] MTCNN P-Net stage not fully implemented yet", node_name.c_str()));
        
        // For now, return empty to allow compilation
        boxes.clear();
    }
    
    void cvedix_mtcnn_face_detector_node::stage2_rnet(
        const cv::Mat& img, 
        std::vector<FaceBox>& boxes) {
        
        // TODO: Implement R-Net stage
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] MTCNN R-Net stage not fully implemented yet", node_name.c_str()));
    }
    
    void cvedix_mtcnn_face_detector_node::stage3_onet(
        const cv::Mat& img, 
        std::vector<FaceBox>& boxes) {
        
        // TODO: Implement O-Net stage
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] MTCNN O-Net stage not fully implemented yet", node_name.c_str()));
    }
    
    void cvedix_mtcnn_face_detector_node::nms(
        std::vector<FaceBox>& boxes, 
        float threshold) {
        
        // Simple NMS implementation
        if (boxes.empty()) {
            return;
        }
        
        // Sort by score (descending)
        std::sort(boxes.begin(), boxes.end(), 
                 [](const FaceBox& a, const FaceBox& b) { 
                     return a.score > b.score; 
                 });
        
        std::vector<bool> suppressed(boxes.size(), false);
        
        for (size_t i = 0; i < boxes.size(); i++) {
            if (suppressed[i]) {
                continue;
            }
            
            const auto& box_i = boxes[i].box;
            float area_i = box_i.width * box_i.height;
            
            for (size_t j = i + 1; j < boxes.size(); j++) {
                if (suppressed[j]) {
                    continue;
                }
                
                const auto& box_j = boxes[j].box;
                
                // Compute intersection
                int x1 = std::max(box_i.x, box_j.x);
                int y1 = std::max(box_i.y, box_j.y);
                int x2 = std::min(box_i.x + box_i.width, box_j.x + box_j.width);
                int y2 = std::min(box_i.y + box_i.height, box_j.y + box_j.height);
                
                int w = std::max(0, x2 - x1);
                int h = std::max(0, y2 - y1);
                float inter = w * h;
                
                float area_j = box_j.width * box_j.height;
                float iou = inter / (area_i + area_j - inter);
                
                if (iou > threshold) {
                    suppressed[j] = true;
                }
            }
        }
        
        // Remove suppressed boxes
        std::vector<FaceBox> result;
        for (size_t i = 0; i < boxes.size(); i++) {
            if (!suppressed[i]) {
                result.push_back(boxes[i]);
            }
        }
        
        boxes = result;
    }
}






