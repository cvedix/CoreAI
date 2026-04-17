/**
 * @file rknn_yolov11_backend.cpp
 * @brief RKNN Backend Plugin Implementation
 */

#include "rknn_yolov26_backend.h"

namespace rknn_yolov26 {

bool rknn_yolov26_backend::detect(const std::vector<cv::Mat>& images,
                                  std::vector<std::vector<cvedix_nodes::infers::Detection>>& detections) {
    // std::cout << "[RKNN Backend] detect() called with " << images.size() << " images" << std::endl;
    
    if (!detector || images.empty()) {
        std::cout << "[RKNN Backend] ERROR: detector is null or no images" << std::endl;
        return false;
    }

    try {
        detections.resize(images.size());
        
        // Run detection on each image (RKNN doesn't support native batch inference)
        for (size_t i = 0; i < images.size(); i++) {
            // Convert BGR to RGB as expected by RKNN detector
            cv::Mat image_rgb;
            if (images[i].channels() == 3) {
                cv::cvtColor(images[i], image_rgb, cv::COLOR_BGR2RGB);
            } else if (images[i].channels() == 4) {
                cv::cvtColor(images[i], image_rgb, cv::COLOR_BGRA2RGB);
            } else {
                image_rgb = images[i].clone();
            }

            // Run RKNN inference
            object_detect_result_list rknn_results;
            // std::cout << "[RKNN Backend] Running inference on image " << i 
            //           << " (" << image_rgb.cols << "x" << image_rgb.rows << ")" 
            //           << " conf=" << conf_threshold << " nms=" << nms_threshold << std::endl;
            int ret = detector->run_inference(image_rgb, &rknn_results, conf_threshold, nms_threshold);
            
            // std::cout << "[RKNN Backend] Inference returned: " << ret 
            //           << ", detections: " << rknn_results.count << std::endl;
            
            if (ret != 0) {
                std::cout << "[RKNN Backend] WARNING: Inference failed, skipping image " << i << std::endl;
                continue;  // Skip this image on failure
            }

            // Convert RKNN results to backend format
            // std::cout << "[RKNN Backend] Converting " << rknn_results.count 
            //           << " detections to backend format" << std::endl;
            
            for (int j = 0; j < rknn_results.count; j++) {
                auto& res = rknn_results.results[j];
                
                cvedix_nodes::infers::Detection det;
                
                // Convert from corner coordinates (left, top, right, bottom) 
                // to center coordinates (cx, cy, w, h) as expected by the node
                float left = res.box.left;
                float top = res.box.top;
                float right = res.box.right;
                float bottom = res.box.bottom;
                
                float w = right - left;
                float h = bottom - top;
                float cx = left + w / 2.0f;
                float cy = top + h / 2.0f;
                
                det.bbox[0] = cx;
                det.bbox[1] = cy;
                det.bbox[2] = w;
                det.bbox[3] = h;
                det.confidence = res.prop;
                det.class_id = res.cls_id;
                
                detections[i].push_back(det);
            }
        }

        // std::cout << "[RKNN Backend] detect() completed successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "[RKNN Backend] ERROR in detect(): " << e.what() << std::endl;
        return false;
    }
}

} // namespace rknn_yolov26

extern "C" {

// Factory function: create backend and initialize with model
// Model path format: "model.rknn?num_classes=80" (query param optional, default=80)
cvedix_nodes::infers::cvedix_infer_detector_backend* create_backend(const char* model_path) {
    // std::cout << "[RKNN Backend] create_backend() called with model: " << model_path << std::endl;
    
    try {
        // Parse model path and optional parameters
        std::string path_str(model_path);
        std::string actual_path = path_str;
        int num_classes = 80;  // default
        
        // Check for query parameters
        size_t query_pos = path_str.find('?');
        if (query_pos != std::string::npos) {
            actual_path = path_str.substr(0, query_pos);
            std::string query = path_str.substr(query_pos + 1);
            
            // Parse num_classes parameter
            size_t nc_pos = query.find("num_classes=");
            if (nc_pos != std::string::npos) {
                num_classes = std::stoi(query.substr(nc_pos + 12));
            }
        }
        
        std::cout << "[RKNN Backend] Creating backend with model: " << actual_path 
                  << ", num_classes: " << num_classes << std::endl;
        
        auto backend = new rknn_yolov26::rknn_yolov26_backend();
        backend->init_detector(actual_path.c_str(), num_classes);
        
        std::cout << "[RKNN Backend] Backend created successfully" << std::endl;
        return backend;
    } catch (const std::exception& e) {
        std::cout << "[RKNN Backend] ERROR creating backend: " << e.what() << std::endl;
        return nullptr;
    }
}

// Destroy function
void destroy_backend(cvedix_nodes::infers::cvedix_infer_detector_backend* backend) {
    delete backend;
}

} // extern "C"
