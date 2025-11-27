#pragma once

#ifdef CVEDIX_WITH_RGA

#include <opencv2/opencv.hpp>
#include <string>

// RGA headers - Rockchip Graphics Accelerator API
// librga provides im2d.h with functions: imresize, imcvtcolor, imcrop, etc.
// Note: im2d.h already handles C/C++ compatibility internally
// We don't wrap in extern "C" to avoid conflicts between C and C++ API declarations
#include "im2d.h"
// rga.h is optional and may cause conflicts, only include if needed
// #include "rga.h"  // Commented out to avoid conflicts

namespace cvedix_utils {
    // RGA helper class for hardware-accelerated image processing on Rockchip
    // Provides resize, color conversion, and crop-resize operations
    class cvedix_rga_helper {
    private:
        bool initialized;
        bool available;
        
        // Check if RGA is available
        bool check_rga_available();
        
        // Helper: Convert OpenCV Mat to RGA buffer
        int mat_to_rga_buffer(const cv::Mat& mat, rga_buffer_t& rga_buf);
        
        // Helper: Get RGA format from OpenCV type
        int get_rga_format(int cv_type);
        
        // Helper: Get RGA color space from OpenCV color conversion code
        int get_rga_color_space(int cv_code);
        
    public:
        cvedix_rga_helper();
        ~cvedix_rga_helper();
        
        // Initialize RGA
        // Returns true if RGA is available and initialized
        bool init();
        
        // Check if RGA is available
        bool is_available() const { return available && initialized; }
        
        // Resize image using RGA (hardware accelerated)
        // src: source image (BGR or RGB)
        // dst: destination image (will be resized)
        // dst_size: target size (width, height)
        // Returns 0 on success, negative on error
        int resize(const cv::Mat& src, cv::Mat& dst, const cv::Size& dst_size);
        
        // Color conversion using RGA
        // src: source image
        // dst: destination image
        // code: conversion code (COLOR_BGR2RGB, COLOR_RGB2BGR, etc.)
        // Returns 0 on success, negative on error
        int cvt_color(const cv::Mat& src, cv::Mat& dst, int code);
        
        // Crop and resize in one operation (more efficient)
        // src: source image
        // dst: destination image
        // crop_rect: rectangle to crop from source (x, y, width, height)
        // dst_size: target size after resize
        // Returns 0 on success, negative on error
        int crop_resize(const cv::Mat& src, cv::Mat& dst, const cv::Rect& crop_rect, const cv::Size& dst_size);
        
        // Resize with aspect ratio preserved (letterbox)
        // src: source image
        // dst: destination image (will be padded to maintain aspect ratio)
        // dst_size: target size
        // Returns 0 on success, negative on error
        int resize_letterbox(const cv::Mat& src, cv::Mat& dst, const cv::Size& dst_size, cv::Scalar pad_color = cv::Scalar(114, 114, 114));
    };
}

#else // CVEDIX_WITH_RGA not defined

// Stub implementation when RGA is not available
#include <opencv2/opencv.hpp>
#include <string>

namespace cvedix_utils {
    // Stub RGA helper class when RGA is not enabled
    class cvedix_rga_helper {
    public:
        cvedix_rga_helper() {}
        ~cvedix_rga_helper() {}
        bool init() { return false; }
        bool is_available() const { return false; }
        int resize(const cv::Mat&, cv::Mat&, const cv::Size&) { return -1; }
        int cvt_color(const cv::Mat&, cv::Mat&, int) { return -1; }
        int crop_resize(const cv::Mat&, cv::Mat&, const cv::Rect&, const cv::Size&) { return -1; }
        int resize_letterbox(const cv::Mat&, cv::Mat&, const cv::Size&, cv::Scalar = cv::Scalar(114, 114, 114)) { return -1; }
    };
}

#endif // CVEDIX_WITH_RGA
