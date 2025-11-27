#include "cvedix_face_yunet_int8_face_detection_mode.h"
#include <algorithm>
#include <chrono>
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"
#include "cvedix/objects/cvedix_frame_target.h"

namespace cvedix_nodes {

    cvedix_face_yunet_int8_face_detection_mode::cvedix_face_yunet_int8_face_detection_mode(
        std::string node_name,
        std::string model_path,
        float conf_threshold,
        float nms_threshold,
        int top_k,
        int input_width,
        int input_height,
        int backend_id,
        int target_id)
        : cvedix_primary_infer_node(node_name,
                                    model_path,
                                    "",
                                    "",
                                    input_width,
                                    input_height,
                                    1),
          conf_threshold(conf_threshold),
          nms_threshold(nms_threshold),
          top_k(top_k),
          backend_id(backend_id),
          target_id(target_id) {

        detector_input_size = cv::Size(input_width, input_height);

        try {
            face_detector = cv::FaceDetectorYN::create(model_path,
                                                       "",
                                                       detector_input_size,
                                                       conf_threshold,
                                                       nms_threshold,
                                                       top_k,
                                                       backend_id,
                                                       target_id);
        } catch (const std::exception& ex) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Failed to create FaceDetectorYN from model: %s | error: %s",
                node_name.c_str(),
                model_path.c_str(),
                ex.what()));
            throw;
        }

        if (face_detector.empty()) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] FaceDetectorYN handle is empty. Model path: %s",
                node_name.c_str(),
                model_path.c_str()));
            throw std::runtime_error("FaceDetectorYN initialization failed");
        }

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] FaceDetectorYN initialized (model=%s, backend=%d, target=%d, conf_threshold=%.2f, nms_threshold=%.2f, top_k=%d)",
            node_name.c_str(),
            model_path.c_str(),
            backend_id,
            target_id,
            conf_threshold,
            nms_threshold,
            top_k));

        this->initialized();
    }

    cvedix_face_yunet_int8_face_detection_mode::~cvedix_face_yunet_int8_face_detection_mode() {
        deinitialized();
    }

    void cvedix_face_yunet_int8_face_detection_mode::run_infer_combinations(
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

        if (frame_meta_with_batch.empty()) {
            return;
        }

        if (face_detector.empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] FaceDetectorYN is not initialized", node_name.c_str()));
            return;
        }

        std::vector<cv::Mat> mats_to_infer;
        auto start_time = std::chrono::system_clock::now();
        prepare(frame_meta_with_batch, mats_to_infer);
        auto prepare_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time);

        if (mats_to_infer.empty()) {
            return;
        }

        // Detection (treat as infer time)
        start_time = std::chrono::system_clock::now();
        const size_t frame_count = std::min(mats_to_infer.size(), frame_meta_with_batch.size());
        for (size_t idx = 0; idx < frame_count; ++idx) {
            detect_faces_on_frame(mats_to_infer[idx], frame_meta_with_batch[idx]);
        }
        auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start_time);

        // We perform parsing inline, so postprocess time is 0
        infer_combinations_time_cost(static_cast<int>(frame_count),
                                     prepare_time.count(),
                                     0,
                                     infer_time.count(),
                                     0);
    }

    void cvedix_face_yunet_int8_face_detection_mode::postprocess(
        const std::vector<cv::Mat>&,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>&) {
        // Intentionally unused because FaceDetectorYN runs synchronously in run_infer_combinations.
    }

    void cvedix_face_yunet_int8_face_detection_mode::detect_faces_on_frame(
        const cv::Mat& frame,
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta) {

        if (!frame_meta) {
            return;
        }

        if (frame.empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Input frame is empty (frame_index=%llu, channel=%d)",
                                                    node_name.c_str(),
                                                    frame_meta->frame_index,
                                                    frame_meta->channel_index));
            return;
        }

        if (detector_input_size != frame.size()) {
            try {
                face_detector->setInputSize(frame.size());
                detector_input_size = frame.size();
            } catch (const std::exception& ex) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Failed to update detector input size to %dx%d: %s",
                    node_name.c_str(),
                    frame.cols,
                    frame.rows,
                    ex.what()));
            }
        }

        cv::Mat faces;
        try {
            face_detector->detect(frame, faces);
        } catch (const std::exception& ex) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] FaceDetectorYN detect failed: %s",
                node_name.c_str(),
                ex.what()));
            return;
        }

        if (faces.empty()) {
            CVEDIX_DEBUG(cvedix_utils::string_format(
                "[%s] No faces detected (frame_index=%llu, channel=%d)",
                node_name.c_str(),
                frame_meta->frame_index,
                frame_meta->channel_index));
            return;
        }

        const int num_detections = faces.rows;
        const int frame_width = frame.cols;
        const int frame_height = frame.rows;

        for (int i = 0; i < num_detections; ++i) {
            float score = faces.at<float>(i, 14);
            if (score < conf_threshold) {
                continue;
            }

            float x = faces.at<float>(i, 0);
            float y = faces.at<float>(i, 1);
            float w = faces.at<float>(i, 2);
            float h = faces.at<float>(i, 3);

            cv::Rect box(static_cast<int>(std::round(x)),
                         static_cast<int>(std::round(y)),
                         static_cast<int>(std::round(w)),
                         static_cast<int>(std::round(h)));

            box.x = std::max(0, std::min(box.x, frame_width - 1));
            box.y = std::max(0, std::min(box.y, frame_height - 1));
            box.width = std::max(1, std::min(box.width, frame_width - box.x));
            box.height = std::max(1, std::min(box.height, frame_height - box.y));

            // Keypoints: right eye, left eye, nose tip, right mouth corner, left mouth corner
            std::vector<std::pair<int, int>> keypoints;
            keypoints.reserve(5);
            for (int kp = 0; kp < 5; ++kp) {
                float kp_x = faces.at<float>(i, 4 + kp * 2);
                float kp_y = faces.at<float>(i, 5 + kp * 2);
                int kp_x_i = static_cast<int>(std::round(kp_x));
                int kp_y_i = static_cast<int>(std::round(kp_y));
                kp_x_i = std::max(0, std::min(kp_x_i, frame_width - 1));
                kp_y_i = std::max(0, std::min(kp_y_i, frame_height - 1));
                keypoints.emplace_back(kp_x_i, kp_y_i);
            }

            auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                box.x,
                box.y,
                box.width,
                box.height,
                score,
                keypoints);

            frame_meta->face_targets.push_back(face_target);

            auto general_target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                box.x,
                box.y,
                box.width,
                box.height,
                0,
                score,
                frame_meta->frame_index,
                frame_meta->channel_index,
                "Face");

            frame_meta->targets.push_back(general_target);
        }

        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] FaceDetectorYN detected %zu faces (frame_index=%llu, channel=%d)",
            node_name.c_str(),
            frame_meta->face_targets.size(),
            frame_meta->frame_index,
            frame_meta->channel_index));
    }
}

