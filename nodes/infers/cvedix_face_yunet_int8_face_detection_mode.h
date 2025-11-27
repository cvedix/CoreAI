#pragma once

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/objects/cvedix_frame_face_target.h"
#include <opencv2/objdetect.hpp>

namespace cvedix_nodes {

    /**
     * @brief Face detector node that leverages OpenCV FaceDetectorYN with the quantized YuNet INT8 ONNX model.
     *        This node bypasses the default cv::dnn::Net pipeline and talks to cv::FaceDetectorYN directly.
     */
    class cvedix_face_yunet_int8_face_detection_mode : public cvedix_primary_infer_node {
    private:
        cv::Ptr<cv::FaceDetectorYN> face_detector;
        cv::Size detector_input_size;
        float conf_threshold;
        float nms_threshold;
        int top_k;
        int backend_id;
        int target_id;

        void detect_faces_on_frame(const cv::Mat& frame,
                                   std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta);

    protected:
        void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;
        void postprocess(const std::vector<cv::Mat>& raw_outputs,
                         const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) override;

    public:
        cvedix_face_yunet_int8_face_detection_mode(
            std::string node_name,
            std::string model_path = "./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx",
            float conf_threshold = 0.9f,
            float nms_threshold = 0.3f,
            int top_k = 5000,
            int input_width = 320,
            int input_height = 320,
            int backend_id = cv::dnn::DNN_BACKEND_OPENCV,
            int target_id = cv::dnn::DNN_TARGET_CPU);
        virtual ~cvedix_face_yunet_int8_face_detection_mode();
    };
}

