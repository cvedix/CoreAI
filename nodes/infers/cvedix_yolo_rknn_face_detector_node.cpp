#ifdef CVEDIX_WITH_RKNN

#include "cvedix_yolo_rknn_face_detector_node.h"
#include "../../utils/logger/cvedix_logger.h"
#include <algorithm>
#include <cmath>

namespace cvedix_nodes {

    cvedix_yolo_rknn_face_detector_node::cvedix_yolo_rknn_face_detector_node(
        std::string node_name, 
        std::string model_path,
        float score_threshold,
        float nms_threshold,
        int input_width,
        int input_height)
        : cvedix_primary_infer_node(node_name, model_path, "", "", input_width, input_height),
          score_threshold(score_threshold),
          nms_threshold(nms_threshold),
          input_width(input_width),
          input_height(input_height) {
        
        // Initialize RKNN helper
        rknn_helper = std::make_shared<cvedix_utils::cvedix_rknn_helper>();
        int ret = rknn_helper->init(model_path);
        if (ret != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to initialize RKNN model: %s", 
                                                     node_name.c_str(), model_path.c_str()));
            throw std::runtime_error("RKNN initialization failed");
        }
        
#ifdef CVEDIX_WITH_RGA
        // Initialize RGA helper
        rga_helper = std::make_shared<cvedix_utils::cvedix_rga_helper>();
        use_rga = rga_helper->init();
        if (use_rga) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] RGA acceleration enabled", node_name.c_str()));
        } else {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] RGA not available, using OpenCV", node_name.c_str()));
        }
#endif
        
        this->initialized();
    }

    cvedix_yolo_rknn_face_detector_node::~cvedix_yolo_rknn_face_detector_node() {
        deinitialized();
    }

    void cvedix_yolo_rknn_face_detector_node::preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer) {
        if (mats_to_infer.empty()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] No input images", node_name.c_str()));
            return;
        }
        
        cv::Mat input_image = mats_to_infer[0];
        cv::Mat processed;
        
#ifdef CVEDIX_WITH_RGA
        if (use_rga && rga_helper->is_available()) {
            // Use RGA for resize (hardware accelerated)
            rga_helper->resize(input_image, processed, cv::Size(input_width, input_height));
            
            // Convert BGR to RGB if needed (YoloV8 typically expects RGB)
            cv::Mat rgb_image;
            rga_helper->cvt_color(processed, rgb_image, cv::COLOR_BGR2RGB);
            processed = rgb_image;
        } else {
            // Fallback to OpenCV
            cv::resize(input_image, processed, cv::Size(input_width, input_height));
            cv::cvtColor(processed, processed, cv::COLOR_BGR2RGB);
        }
#else
        // Use OpenCV
        cv::resize(input_image, processed, cv::Size(input_width, input_height));
        cv::cvtColor(processed, processed, cv::COLOR_BGR2RGB);
#endif
        
        // Normalize to [0, 1] and convert to float32
        processed.convertTo(processed, CV_32F, 1.0 / 255.0);
        
        // Create blob (NCHW format for RKNN)
        // RKNN typically expects NCHW format: [batch, channels, height, width]
        std::vector<cv::Mat> channels;
        cv::split(processed, channels);
        
        // Reshape to NCHW: [1, 3, H, W]
        int total_size = 1 * 3 * input_height * input_width;
        blob_to_infer = cv::Mat(1, total_size, CV_32F);
        
        float* blob_data = (float*)blob_to_infer.data;
        int channel_size = input_height * input_width;
        
        for (int c = 0; c < 3; c++) {
            memcpy(blob_data + c * channel_size, channels[c].data, channel_size * sizeof(float));
        }
    }

    void cvedix_yolo_rknn_face_detector_node::infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs) {
        if (!rknn_helper || !rknn_helper->is_initialized()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] RKNN helper not initialized", node_name.c_str()));
            return;
        }
        
        // Convert blob to cv::Mat for RKNN input
        // RKNN expects input in specific format based on model
        cv::Mat input_mat = blob_to_infer.reshape(1, {1, 3, input_height, input_width});
        
        // Set input to RKNN
        int ret = rknn_helper->set_input(0, input_mat);
        if (ret != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to set RKNN input", node_name.c_str()));
            return;
        }
        
        // Run inference
        ret = rknn_helper->run();
        if (ret != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] RKNN inference failed", node_name.c_str()));
            return;
        }
        
        // Get outputs
        int num_outputs = rknn_helper->get_output_num();
        raw_outputs.clear();
        
        for (int i = 0; i < num_outputs; i++) {
            int output_size;
            float* output_data = rknn_helper->get_output(i, output_size);
            if (output_data == nullptr) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] Failed to get output %d", node_name.c_str(), i));
                continue;
            }
            
            // Get output shape from RKNN
            auto output_attr = rknn_helper->get_output_attr(i);
            std::vector<int> shape;
            for (int d = 0; d < output_attr.n_dims; d++) {
                shape.push_back(output_attr.dims[d]);
            }
            
            // Create cv::Mat from output data
            cv::Mat output_mat = rknn_helper->get_output_mat(i, shape);
            raw_outputs.push_back(output_mat);
        }
    }

    void cvedix_yolo_rknn_face_detector_node::parse_yolov8_output(
        float* output_data, 
        int output_size,
        const cv::Size& frame_size,
        std::vector<cv::Rect>& boxes,
        std::vector<float>& scores) {
        
        // YoloV8 output format: [num_detections, 6] where 6 = [x_center, y_center, width, height, conf, class]
        // Or: [1, num_boxes, 85] for standard YOLO format
        // This implementation assumes output is flattened: [num_boxes * 6]
        
        boxes.clear();
        scores.clear();
        
        // Assuming output format: [num_boxes, 6] where 6 = [x, y, w, h, conf, class]
        // Or it could be [1, num_boxes, 85] for YOLO format with 80 classes
        // For face detection, we typically have 1 class, so format might be [num_boxes, 6]
        
        int num_boxes = output_size / 6;  // Assuming 6 values per box
        
        float scale_x = (float)frame_size.width / input_width;
        float scale_y = (float)frame_size.height / input_height;
        
        for (int i = 0; i < num_boxes; i++) {
            float* box_data = output_data + i * 6;
            
            float x_center = box_data[0];
            float y_center = box_data[1];
            float width = box_data[2];
            float height = box_data[3];
            float conf = box_data[4];
            float cls = box_data[5];
            
            // Filter by confidence
            if (conf < score_threshold) {
                continue;
            }
            
            // Convert from center format to top-left format
            int x = (int)((x_center - width / 2) * scale_x);
            int y = (int)((y_center - height / 2) * scale_y);
            int w = (int)(width * scale_x);
            int h = (int)(height * scale_y);
            
            // Clamp to image bounds
            x = std::max(0, std::min(x, frame_size.width - 1));
            y = std::max(0, std::min(y, frame_size.height - 1));
            w = std::max(1, std::min(w, frame_size.width - x));
            h = std::max(1, std::min(h, frame_size.height - y));
            
            boxes.push_back(cv::Rect(x, y, w, h));
            scores.push_back(conf);
        }
    }

    void cvedix_yolo_rknn_face_detector_node::postprocess(
        const std::vector<cv::Mat>& raw_outputs,
        const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) {
        
        if (raw_outputs.empty() || frame_meta_with_batch.empty()) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Empty outputs or frame meta", node_name.c_str()));
            return;
        }
        
        auto& frame_meta = frame_meta_with_batch[0];
        cv::Mat& frame = frame_meta->frame;
        
        // Get output data
        // YoloV8 typically has one output tensor
        cv::Mat output = raw_outputs[0];
        float* output_data = (float*)output.data;
        int output_size = output.total();
        
        // Parse YoloV8 output
        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        parse_yolov8_output(output_data, output_size, frame.size(), boxes, scores);
        
        if (boxes.empty()) {
            return;
        }
        
        // Apply NMS
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, scores, score_threshold, nms_threshold, indices);
        
        // Create face targets
        for (int idx : indices) {
            const cv::Rect& box = boxes[idx];
            float score = scores[idx];
            
            // Create face target (no keypoints for YoloV8, can be added if model supports it)
            auto face_target = std::make_shared<cvedix_objects::cvedix_frame_face_target>(
                box.x, box.y, box.width, box.height, score);
            
            frame_meta->face_targets.push_back(face_target);
        }
        
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Detected %zu faces", 
                                                node_name.c_str(), frame_meta->face_targets.size()));
    }

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_RKNN

