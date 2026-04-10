#pragma once

#include <sstream>
#include <opencv2/opencv.hpp>

#include "cvedix_console_broker_node.h"
#include "../../third_party/cpp_base64/base64.h"

namespace cvedix_nodes {
    // Enhanced message broker node with base64 encoded images
    // Outputs JSON with:
    // - Base64 encoded full frame
    // - Base64 encoded crop images for each target
    // - Bounding box coordinates
    // - Confidence scores
    // - Class names
    // - Track IDs and other related information
    class cvedix_enhanced_console_broker_node: public cvedix_console_broker_node
    {
    private:
        // Flag to enable/disable full frame encoding (can be memory intensive)
        bool encode_full_frame = false;
        
        // Helper function to encode cv::Mat to base64 string
        std::string mat_to_base64(const cv::Mat& img, const std::string& ext = ".jpg");
        
        // Helper function to crop image from frame
        cv::Mat crop_image(const cv::Mat& frame, int x, int y, int width, int height);
        
    protected:
        // Override format_msg to add base64 encoded images
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        
    public:
        cvedix_enhanced_console_broker_node(std::string node_name, 
                                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                                int broking_cache_warn_threshold = 50, 
                                                int broking_cache_ignore_threshold = 200,
                                                bool encode_full_frame = false);
        ~cvedix_enhanced_console_broker_node();
    };
}

