

#include "cvedix_rknn_yolov11_detector_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"
#include "cvedix/objects/cvedix_frame_face_target.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace cvedix_nodes {

    cvedix_rknn_yolov11_detector_node::cvedix_rknn_yolov11_detector_node(
        std::string node_name,
        std::string model_path,
        float score_threshold,
        float nms_threshold,
        int input_width,
        int input_height,
        int num_classes,
        std::string labels_path,
        int class_id_offset)
        : cvedix_primary_infer_node(node_name,
                                    model_path,
                                    "",
                                    labels_path,
                                    input_width,
                                    input_height,
                                    1,
                                    class_id_offset),
          score_threshold(score_threshold),
          nms_threshold(nms_threshold),
          input_width(input_width),
          input_height(input_height),
          num_classes(num_classes) {

        detector = std::make_shared<rknn_yolov11::rknn_yolov11_detector>(model_path, num_classes);
        int ret = detector->init();
        if (ret != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Failed to initialize RKNN YOLOv11 detector: %s (ret=%d)",
                node_name.c_str(),
                model_path.c_str(),
                ret));
            throw std::runtime_error("RKNN YOLOv11 initialization failed");
        }
        
        CVEDIX_INFO(cvedix_utils::string_format("[%s] RKNN YOLOv11 detector initialized successfully", node_name.c_str()));
        this->initialized();
    }

    cvedix_rknn_yolov11_detector_node::~cvedix_rknn_yolov11_detector_node() {
        deinitialized();
    }

    void cvedix_rknn_yolov11_detector_node::preprocess(const std::vector<cv::Mat>& mats_to_infer,
                                                       cv::Mat& blob_to_infer) {
        if (mats_to_infer.empty() || mats_to_infer[0].empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] No input image for preprocessing", node_name.c_str()));
            return;
        }

        std::lock_guard<std::mutex> guard(process_mutex);
        // Clone and convert to RGB as expected by the detector
        current_frame = mats_to_infer[0].clone();
        if (current_frame.channels() == 3) {
            cv::cvtColor(current_frame, current_frame, cv::COLOR_BGR2RGB);
        } else if (current_frame.channels() == 4) {
            cv::cvtColor(current_frame, current_frame, cv::COLOR_BGRA2RGB);
        }

        // Provide a dummy blob to satisfy base class checks (if any)
        blob_to_infer = cv::Mat(1, 1, CV_8UC1, cv::Scalar(0));
    }

    void cvedix_rknn_yolov11_detector_node::infer(const cv::Mat& blob_to_infer,
                                                  std::vector<cv::Mat>& raw_outputs) {
        std::lock_guard<std::mutex> guard(process_mutex);
        if (current_frame.empty()) return;

        // Call the new detector
        int ret = detector->run_inference(current_frame, &current_results, score_threshold, nms_threshold);
        
        if (ret != 0) {
             CVEDIX_ERROR(cvedix_utils::string_format("[%s] Inference failed with code %d", node_name.c_str(), ret));
             current_results.count = 0;
        }

        // Provide dummy raw_outputs to satisfy base class checks (if any)
        raw_outputs.push_back(cv::Mat(1, 1, CV_8UC1, cv::Scalar(0)));
    }

    void cvedix_rknn_yolov11_detector_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {

        std::lock_guard<std::mutex> guard(process_mutex);
        if (frame_meta_with_batch.empty()) return;

        auto& frame_meta = frame_meta_with_batch[0];
        cv::Mat& frame = frame_meta->frame; // Used for drawing if needed, but targets store coordinates
        
        // Process results from the detector
        for (int i = 0; i < current_results.count; ++i) {
            auto& res = current_results.results[i];
            
            int x = res.box.left;
            int y = res.box.top;
            int w = res.box.right - res.box.left;
            int h = res.box.bottom - res.box.top;
            int cls = res.cls_id;
            float score = res.prop;
            
            int global_class_id = cls + class_id_offset;
            
            std::string label;
            if (!labels.empty() && cls >= 0 && cls < static_cast<int>(labels.size())) {
                label = labels[cls];
            }

            // Clip to frame dimensions
            x = std::max(0, x);
            y = std::max(0, y);
            w = std::min(w, frame_meta->frame.cols - x);
            h = std::min(h, frame_meta->frame.rows - y);

            if (w <= 0 || h <= 0) continue;

            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                x, y, w, h,
                global_class_id,
                score,
                frame_meta->frame_index,
                frame_meta->channel_index,
                label);

            frame_meta->targets.push_back(target);

            // Draw debug
            if (!frame.empty()) {
                cv::Rect box(x, y, w, h);
                cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
                
                std::ostringstream score_stream;
                score_stream << std::fixed << std::setprecision(2) << score;
                std::string caption = label.empty() ? score_stream.str() : (label + " " + score_stream.str());

                cv::Size text_size = cv::getTextSize(caption, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, nullptr);
                cv::Point text_origin(box.x, std::max(0, box.y - 4));
                if (text_origin.y - text_size.height < 0) {
                    text_origin.y = std::min(frame.rows - 1, box.y + text_size.height + 4);
                }
                cv::putText(frame, caption, text_origin, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
            }
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Detected %d objects", node_name.c_str(), current_results.count));
    }

} // namespace cvedix_nodes
