#ifdef CVEDIX_WITH_TRT
#include "cvedix_trt_insight_face_recognition_node.h"
#include <algorithm>
#include <opencv2/imgproc.hpp>

namespace cvedix_nodes {
        
    cvedix_trt_insight_face_recognition_node::cvedix_trt_insight_face_recognition_node(
        std::string node_name, 
        std::string model_path,
        int input_width,
        int input_height,
        bool enable_alignment):
        cvedix_secondary_infer_node(node_name, "", "", "", 
                                   input_width, input_height, 
                                   1, std::vector<int>(), 0, 0),
        enable_alignment(enable_alignment) {
        recognizer = std::make_shared<trt_insightface::InsightFaceRecognition>(model_path);
        this->initialized();
    }
    
    cvedix_trt_insight_face_recognition_node::~cvedix_trt_insight_face_recognition_node() {
        deinitialized();
    }

    void cvedix_trt_insight_face_recognition_node::prepare(
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
                    {face_target->key_points[0].first, face_target->key_points[0].second}, 
                    {face_target->key_points[1].first, face_target->key_points[1].second}, 
                    {face_target->key_points[2].first, face_target->key_points[2].second}, 
                    {face_target->key_points[3].first, face_target->key_points[3].second}, 
                    {face_target->key_points[4].first, face_target->key_points[4].second}
                };
                alignCrop(frame_meta->frame, face_keypoints, face_img);
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
                
                face_img = frame_meta->frame(face_rect);
                
                // Resize to input size if needed
                if (face_img.rows != input_height || face_img.cols != input_width) {
                    cv::resize(face_img, face_img, cv::Size(input_width, input_height), 
                              0, 0, cv::INTER_LINEAR);
                }
            }
            
            mats_to_infer.push_back(face_img);
        }
    }

    void cvedix_trt_insight_face_recognition_node::run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        
        assert(frame_meta_with_batch.size() == 1);
        std::vector<cv::Mat> mats_to_infer;

        // Start timing
        auto start_time = std::chrono::system_clock::now();

        // Prepare data (align & crop faces)
        prepare(frame_meta_with_batch, mats_to_infer);
        auto prepare_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time);

        if (mats_to_infer.empty()) {
            return;
        }

        // Run TensorRT inference
        start_time = std::chrono::system_clock::now();
        std::vector<std::vector<float>> embeddings;
        recognizer->extract_features(mats_to_infer, embeddings);
        auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time);

        // Map embeddings back to face_targets
        auto& frame_meta = frame_meta_with_batch[0];
        assert(embeddings.size() == frame_meta->face_targets.size());

        for (size_t i = 0; i < embeddings.size(); i++) {
            frame_meta->face_targets[i]->embeddings = embeddings[i];
        }

        // Record timing (preprocess and postprocess set to 0 as they're combined)
        cvedix_infer_node::infer_combinations_time_cost(
            mats_to_infer.size(), prepare_time.count(), 0, infer_time.count(), 0);
    }

    void cvedix_trt_insight_face_recognition_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        // Not used - inference is handled in run_infer_combinations
    }

    // Face alignment methods (ported from cvedix_sface_feature_encoder_node)
    cv::Mat cvedix_trt_insight_face_recognition_node::getSimilarityTransformMatrix(float src[5][2]) {
        using namespace cv;
        // Standard face alignment landmarks (for 112x112 output)
        float dst[5][2] = { 
            {38.2946f, 51.6963f}, 
            {73.5318f, 51.5014f}, 
            {56.0252f, 71.7366f}, 
            {41.5493f, 92.3655f}, 
            {70.7299f, 92.2041f} 
        };
        
        float avg0 = (src[0][0] + src[1][0] + src[2][0] + src[3][0] + src[4][0]) / 5;
        float avg1 = (src[0][1] + src[1][1] + src[2][1] + src[3][1] + src[4][1]) / 5;
        
        // Compute mean of src and dst
        float src_mean[2] = { avg0, avg1 };
        float dst_mean[2] = { 56.0262f, 71.9008f };
        
        // Subtract mean from src and dst
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
        A00 = A00 / 5;
        A01 = A01 / 5;
        A10 = A10 / 5;
        A11 = A11 / 5;
        
        Mat A = (Mat_<double>(2, 2) << A00, A01, A10, A11);
        double d[2] = { 1.0, 1.0 };
        double detA = A00 * A11 - A01 * A10;
        if (detA < 0)
            d[1] = -1;
        
        double T[3][3] = { {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0} };
        Mat s, u, vt, v;
        SVD::compute(A, s, u, vt);
        double smax = s.ptr<double>(0)[0] > s.ptr<double>(1)[0] ? s.ptr<double>(0)[0] : s.ptr<double>(1)[0];
        double tol = smax * 2 * FLT_MIN;
        int rank = 0;
        if (s.ptr<double>(0)[0] > tol)
            rank += 1;
        if (s.ptr<double>(1)[0] > tol)
            rank += 1;
        
        double arr_u[2][2] = { {u.ptr<double>(0)[0], u.ptr<double>(0)[1]}, {u.ptr<double>(1)[0], u.ptr<double>(1)[1]} };
        double arr_vt[2][2] = { {vt.ptr<double>(0)[0], vt.ptr<double>(0)[1]}, {vt.ptr<double>(1)[0], vt.ptr<double>(1)[1]} };
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
                Mat Dvt = D * vt;
                Mat uDvt = u * Dvt;
                T[0][0] = uDvt.ptr<double>(0)[0];
                T[0][1] = uDvt.ptr<double>(0)[1];
                T[1][0] = uDvt.ptr<double>(1)[0];
                T[1][1] = uDvt.ptr<double>(1)[1];
                d[1] = temp;
            }
        } else {
            Mat D = (Mat_<double>(2, 2) << d[0], 0.0, 0.0, d[1]);
            Mat Dvt = D * vt;
            Mat uDvt = u * Dvt;
            T[0][0] = uDvt.ptr<double>(0)[0];
            T[0][1] = uDvt.ptr<double>(0)[1];
            T[1][0] = uDvt.ptr<double>(1)[0];
            T[1][1] = uDvt.ptr<double>(1)[1];
        }
        
        double var1 = 0.0;
        for (int i = 0; i < 5; i++) {
            var1 += src_demean[i][0] * src_demean[i][0];
        }
        var1 = var1 / 5;
        double var2 = 0.0;
        for (int i = 0; i < 5; i++) {
            var2 += src_demean[i][1] * src_demean[i][1];
        }
        var2 = var2 / 5;
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

    void cvedix_trt_insight_face_recognition_node::alignCrop(
        cv::Mat& _src_img, 
        float _src_point[5][2], 
        cv::Mat& _aligned_img) {
        cv::Mat warp_mat = getSimilarityTransformMatrix(_src_point);
        cv::warpAffine(_src_img, _aligned_img, warp_mat, 
                      cv::Size(input_width, input_height), cv::INTER_LINEAR);
    }
}

#endif  // CVEDIX_WITH_TRT






