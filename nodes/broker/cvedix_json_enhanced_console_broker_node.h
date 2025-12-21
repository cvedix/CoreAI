/**
 * @file cvedix_json_enhanced_console_broker_node.h
 * @brief Enhanced debug broker with base64 encoded images
 * 
 * Extends cvedix_json_console_broker_node to include base64 encoded images.
 * 
 * @section enhanced_features Features
 * - Base64 encoded crop images for each target
 * - Optional base64 encoded full frame
 * - Bounding box, confidence, class names, track IDs
 * 
 * @section enhanced_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_json_enhanced_console_broker_node>(
 *     "debug_enhanced",
 *     cvedix_broke_for::NORMAL,
 *     50, 200,
 *     true  // encode full frame (memory intensive)
 * );
 * @endcode
 * 
 * @see cvedix_json_console_broker_node Base class (text-only)
 */

#pragma once

#include <sstream>
#include <opencv2/opencv.hpp>

#include "cvedix_json_console_broker_node.h"
#include "cvedix/third_party/cpp_base64/base64.h"

namespace cvedix_nodes {

    /**
     * @brief Enhanced debug broker with base64 images
     * 
     * Outputs JSON with embedded base64 images for each target.
     * Useful for web-based debugging interfaces.
     * 
     * @see cvedix_json_console_broker_node Base class
     */
    class cvedix_json_enhanced_console_broker_node: public cvedix_json_console_broker_node
    {
    private:
        /// @brief Enable full frame base64 encoding (memory intensive)
        bool encode_full_frame = false;
        
        /**
         * @brief Encode cv::Mat to base64 string
         * @param img Image to encode
         * @param ext Image extension (default: ".jpg")
         * @return Base64 encoded string
         */
        std::string mat_to_base64(const cv::Mat& img, const std::string& ext = ".jpg");
        
        /**
         * @brief Crop region from frame
         * @param frame Source frame
         * @param x, y Top-left coordinates
         * @param width, height Crop dimensions
         * @return Cropped image
         */
        cv::Mat crop_image(const cv::Mat& frame, int x, int y, int width, int height);
        
    protected:
        /**
         * @brief Format with base64 images
         * @param meta Frame meta with targets
         * @param[out] msg Output JSON with embedded images
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        
    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param broke_for Target type
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         * @param encode_full_frame Include full frame base64 (default: false)
         */
        cvedix_json_enhanced_console_broker_node(std::string node_name, 
                                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                                int broking_cache_warn_threshold = 50, 
                                                int broking_cache_ignore_threshold = 200,
                                                bool encode_full_frame = false);

        /// @brief Destructor
        ~cvedix_json_enhanced_console_broker_node();
    };
}
