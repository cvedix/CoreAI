#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <opencv2/opencv.hpp>

namespace cvedix_nodes::infers {

/**
 * @brief Detection result structure
 */
struct Detection {
    float bbox[4];          
    float confidence;       // confidence score [0, 1]
    int32_t class_id;       // class ID (0-based)
    std::string label;      // class label (optional)
};

/**
 * @brief Backend interface for inference engines
 * 
 * All inference backends (TensorRT, OpenVINO, etc.) must implement this interface.
 * Simple and minimal - only essential methods.
 */
class cvedix_infer_detector_backend {
public:
    virtual ~cvedix_infer_detector_backend() = default;

    /// @brief Run inference on batch of images
    /// @param images Vector of cv::Mat images (BGR format, uint8_t)
    /// @param detections Output detections for each image
    /// @return true if inference succeeded
    virtual bool detect(const std::vector<cv::Mat>& images,
                       std::vector<std::vector<Detection>>& detections) = 0;

    /// @brief Get input width required by model
    virtual int get_input_width() const = 0;

    /// @brief Get input height required by model
    virtual int get_input_height() const = 0;

    /// @brief Set confidence threshold for filtering detections
    virtual void set_conf_threshold(float threshold) = 0;

    /// @brief Set NMS threshold for suppression
    virtual void set_nms_threshold(float threshold) = 0;

    /// @brief Cleanup and destroy backend resources
    virtual void destroy() = 0;
};

} // namespace cvedix_nodes::infers
