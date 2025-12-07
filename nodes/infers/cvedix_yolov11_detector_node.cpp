#include "cvedix_yolov11_detector_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix/utils/cvedix_utils.h"
#include <algorithm>
#include <cmath>

namespace cvedix_nodes {

    cvedix_yolov11_detector_node::cvedix_yolov11_detector_node(
        std::string node_name,
        std::string model_path,
        std::string labels_path,
        int input_width,
        int input_height,
        int num_classes,
        float score_threshold,
        float nms_threshold,
        int class_id_offset)
        : cvedix_primary_infer_node(node_name,
                                    model_path,
                                    "",  // No config file for ONNX
                                    labels_path,
                                    input_width,
                                    input_height,
                                    1,  // Batch size
                                    class_id_offset,
                                    1.0/255.0,  // Scale to [0,1]
                                    cv::Scalar(0),  // No mean subtraction
                                    cv::Scalar(1),  // No std normalization
                                    true),  // Swap RB (BGR to RGB)
          score_threshold(score_threshold),
          nms_threshold(nms_threshold),
          num_classes(num_classes) {
        
        // Load ONNX model explicitly
        try {
            net = cv::dnn::readNetFromONNX(model_path);
            
            if (net.empty()) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] Failed to load ONNX model: %s",
                    node_name.c_str(), model_path.c_str()));
                throw std::runtime_error("Failed to load ONNX model");
            }
            
            // Set backend (use CUDA if available)
            #ifdef CVEDIX_WITH_CUDA
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Using CUDA backend", node_name.c_str()));
            #else
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            #endif
            
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] YOLOv11 ONNX detector initialized: %dx%d, %d classes, score_thresh=%.2f, nms_thresh=%.2f",
                node_name.c_str(), input_width, input_height, num_classes, score_threshold, nms_threshold));
        }
        catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Exception loading ONNX model: %s - %s",
                node_name.c_str(), model_path.c_str(), e.what()));
            throw;
        }
        
        this->initialized();
    }

    cvedix_yolov11_detector_node::~cvedix_yolov11_detector_node() {
        deinitialized();
    }

    void cvedix_yolov11_detector_node::process_output(
        const cv::Mat& output,
        const cv::Size& frame_size,
        std::vector<int>& class_ids,
        std::vector<float>& confidences,
        std::vector<cv::Rect>& boxes) {
        
        // YOLOv11 output format: [batch, 4+num_classes, num_boxes]
        // output.dims = 3: [1, 84, 8400] for 80 classes
        // First 4 channels: x, y, w, h (center coordinates, normalized)
        // Remaining channels: class probabilities
        
        int dimensions = output.size[1];  // 4 + num_classes
        int num_boxes = output.size[2];
        
        const float* data = (float*)output.data;
        
        for (int i = 0; i < num_boxes; i++) {
            // Get class scores
            float max_class_score = 0.0f;
            int best_class_id = 0;
            
            for (int c = 0; c < num_classes; c++) {
                float score = data[(4 + c) * num_boxes + i];
                if (score > max_class_score) {
                    max_class_score = score;
                    best_class_id = c;
                }
            }
            
            // Filter by score threshold
            if (max_class_score < score_threshold) {
                continue;
            }
            
            // Get bbox (normalized coordinates)
            float cx = data[0 * num_boxes + i];
            float cy = data[1 * num_boxes + i];
            float w = data[2 * num_boxes + i];
            float h = data[3 * num_boxes + i];
            
            // Convert to pixel coordinates
            int left = static_cast<int>((cx - w / 2.0f) * frame_size.width);
            int top = static_cast<int>((cy - h / 2.0f) * frame_size.height);
            int width = static_cast<int>(w * frame_size.width);
            int height = static_cast<int>(h * frame_size.height);
            
            class_ids.push_back(best_class_id);
            confidences.push_back(max_class_score);
            boxes.push_back(cv::Rect(left, top, width, height));
        }
    }

    void cvedix_yolov11_detector_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        
        if (raw_outputs.empty() || frame_meta_with_batch.empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Empty outputs or frame_meta", node_name.c_str()));
            return;
        }
        
        // YOLOv11 typically has 1 output
        const cv::Mat& output = raw_outputs[0];
        auto& frame_meta = frame_meta_with_batch[0];
        
        std::vector<int> class_ids;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;
        
        // Process output to get detections
        process_output(output, frame_meta->frame.size(), class_ids, confidences, boxes);
        
        // Apply NMS
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, score_threshold, nms_threshold, indices);
        
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Detected %d objects before NMS, %d after NMS",
            node_name.c_str(), (int)boxes.size(), (int)indices.size()));
        
        // Create targets from NMS results
        for (int idx : indices) {
            cv::Rect box = boxes[idx];
            float confidence = confidences[idx];
            int class_id = class_ids[idx];
            
            // Clip to frame boundaries
            box.x = std::max(0, box.x);
            box.y = std::max(0, box.y);
            box.width = std::min(box.width, frame_meta->frame.cols - box.x);
            box.height = std::min(box.height, frame_meta->frame.rows - box.y);
            
            if (box.width <= 0 || box.height <= 0) {
                continue;
            }
            
            // Get label
            std::string label = "";
            if (!labels.empty() && class_id >= 0 && class_id < static_cast<int>(labels.size())) {
                label = labels[class_id];
            }
            
            // Apply class ID offset
            int global_class_id = class_id + class_id_offset;
            
            // Create target
            auto target = std::make_shared<cvedix_objects::cvedix_frame_target>(
                box.x, box.y, box.width, box.height,
                global_class_id,
                confidence,
                frame_meta->frame_index,
                frame_meta->channel_index,
                label
            );
            
            frame_meta->targets.push_back(target);
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] Added %d targets to frame_meta",
            node_name.c_str(), (int)indices.size()));
    }

} // namespace cvedix_nodes

