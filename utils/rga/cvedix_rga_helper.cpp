#ifdef CVEDIX_WITH_RGA

#include "cvedix_rga_helper.h"
#include "../logger/cvedix_logger.h"
#include <cstring>
#include <dlfcn.h>

namespace cvedix_utils {

    // Helper: Get RGA format from OpenCV Mat type
    int cvedix_rga_helper::get_rga_format(int cv_type) {
        int depth = CV_MAT_DEPTH(cv_type);
        int channels = CV_MAT_CN(cv_type);
        
        if (depth == CV_8U) {
            switch (channels) {
                case 1: return RK_FORMAT_YCbCr_400;  // Grayscale format
                case 3: return RK_FORMAT_BGR_888;     // OpenCV uses BGR by default
                case 4: return RK_FORMAT_BGRA_8888;   // OpenCV uses BGRA by default
                default: return -1;
            }
        }
        return -1;
    }

    // Helper: Get RGA color space conversion
    // Note: COLOR_BGR2RGB and COLOR_RGB2BGR have the same value (4), so we handle them together
    int cvedix_rga_helper::get_rga_color_space(int cv_code) {
        // For BGR<->RGB swap, we use format conversion instead of color space mode
        // RGA can convert between RK_FORMAT_BGR_888 and RK_FORMAT_RGB_888
        if (cv_code == cv::COLOR_BGR2RGB || cv_code == cv::COLOR_RGB2BGR) {
            return IM_COLOR_SPACE_DEFAULT;  // Will handle format conversion separately
        }
        // For other conversions, use default color space
        if (cv_code == cv::COLOR_BGR2GRAY || cv_code == cv::COLOR_RGB2GRAY) {
            return IM_COLOR_SPACE_DEFAULT;
        }
        return IM_COLOR_SPACE_DEFAULT;
    }

    // Helper: Convert OpenCV Mat to RGA buffer
    int cvedix_rga_helper::mat_to_rga_buffer(const cv::Mat& mat, rga_buffer_t& rga_buf) {
        int format = get_rga_format(mat.type());
        if (format < 0) {
            CVEDIX_ERROR("[RGA] Unsupported OpenCV format for RGA conversion");
            return -1;
        }
        
        // wrapbuffer_virtualaddr is a macro that returns rga_buffer_t directly
        rga_buf = wrapbuffer_virtualaddr((void*)mat.data, mat.cols, mat.rows, format);
        if (rga_buf.vir_addr == nullptr) {
            CVEDIX_ERROR("[RGA] Failed to wrap buffer");
            return -1;
        }
        
        return 0;
    }

    cvedix_rga_helper::cvedix_rga_helper() 
        : initialized(false), available(false) {
    }

    cvedix_rga_helper::~cvedix_rga_helper() {
    }

    bool cvedix_rga_helper::check_rga_available() {
        // Try to test RGA with a simple operation
        // Create a small test image and try to resize it
        try {
            cv::Mat test_src(100, 100, CV_8UC3);
            cv::Mat test_dst(50, 50, CV_8UC3);
            
            // wrapbuffer_virtualaddr is a macro that returns rga_buffer_t directly
            rga_buffer_t src_buf = wrapbuffer_virtualaddr((void*)test_src.data, test_src.cols, test_src.rows, RK_FORMAT_BGR_888);
            rga_buffer_t dst_buf = wrapbuffer_virtualaddr((void*)test_dst.data, test_dst.cols, test_dst.rows, RK_FORMAT_BGR_888);
            
            // Check if buffers are valid
            if (src_buf.vir_addr == nullptr || dst_buf.vir_addr == nullptr) {
                return false;
            }
            
            // Try a simple resize operation using C API to avoid conflicts
            int ret = imresize_t(src_buf, dst_buf, 0, 0, INTER_LINEAR, 1);
            
            if (ret == IM_STATUS_SUCCESS) {
                return true;
            }
        } catch (...) {
            // If any exception, RGA is not available
            return false;
        }
        
        return false;
    }

    bool cvedix_rga_helper::init() {
        if (initialized) {
            return available;
        }
        
        available = check_rga_available();
        initialized = true;
        
        if (available) {
            CVEDIX_INFO("[RGA] RGA helper initialized successfully");
        } else {
            CVEDIX_WARN("[RGA] RGA not available, will fallback to OpenCV");
        }
        
        return available;
    }

    int cvedix_rga_helper::resize(const cv::Mat& src, cv::Mat& dst, const cv::Size& dst_size) {
        if (!is_available()) {
            // Fallback to OpenCV
            cv::resize(src, dst, dst_size);
            return 0;
        }
        
        // Ensure destination Mat is allocated with correct size and type
        if (dst.empty() || dst.size() != dst_size || dst.type() != src.type()) {
            dst = cv::Mat(dst_size, src.type());
        }
        
        // RGA works directly with image data
        // wrapbuffer_virtualaddr is a macro that returns rga_buffer_t directly
        rga_buffer_t src_buf = wrapbuffer_virtualaddr(
            (void*)src.data, src.cols, src.rows, get_rga_format(src.type()));
        rga_buffer_t dst_buf = wrapbuffer_virtualaddr(
            (void*)dst.data, dst.cols, dst.rows, get_rga_format(dst.type()));
        
        // Check if buffers are valid
        if (src_buf.vir_addr == nullptr || dst_buf.vir_addr == nullptr) {
            CVEDIX_WARN("[RGA] Failed to wrap buffers, falling back to OpenCV");
            cv::resize(src, dst, dst_size);
            return 0;
        }
        
        // Perform hardware-accelerated resize using C API to avoid C++/C conflicts
        int ret = imresize_t(src_buf, dst_buf, 0, 0, INTER_LINEAR, 1);
        
        if (ret != IM_STATUS_SUCCESS) {
            CVEDIX_WARN(cvedix_utils::string_format("[RGA] Resize failed: %s, falling back to OpenCV", 
                                                    imStrError((IM_STATUS)ret)));
            cv::resize(src, dst, dst_size);
            return 0;
        }
        
        return 0;
    }

    int cvedix_rga_helper::cvt_color(const cv::Mat& src, cv::Mat& dst, int code) {
        if (!is_available()) {
            // Fallback to OpenCV
            cv::cvtColor(src, dst, code);
            return 0;
        }
        
        // RGA supports limited color conversions
        // Main use case: BGR <-> RGB swap (very common in ML pipelines)
        if (code == cv::COLOR_BGR2RGB || code == cv::COLOR_RGB2BGR) {
            // Ensure destination Mat is allocated
            if (dst.empty() || dst.size() != src.size() || dst.type() != src.type()) {
                dst = cv::Mat(src.size(), src.type());
            }
            
            // wrapbuffer_virtualaddr is a macro that returns rga_buffer_t directly
            rga_buffer_t src_buf = wrapbuffer_virtualaddr(
                (void*)src.data, src.cols, src.rows, get_rga_format(src.type()));
            rga_buffer_t dst_buf = wrapbuffer_virtualaddr(
                (void*)dst.data, dst.cols, dst.rows, get_rga_format(dst.type()));
            
            // Check if buffers are valid
            if (src_buf.vir_addr == nullptr || dst_buf.vir_addr == nullptr) {
                CVEDIX_WARN("[RGA] Failed to wrap buffers for color conversion, falling back to OpenCV");
                cv::cvtColor(src, dst, code);
                return 0;
            }
            
            // Use RGA to swap R and B channels (hardware accelerated)
            // Convert BGR to RGB by using different format
            int src_fmt = get_rga_format(src.type());
            int dst_fmt;
            if (src_fmt == RK_FORMAT_BGR_888) {
                dst_fmt = RK_FORMAT_RGB_888;  // BGR -> RGB
            } else if (src_fmt == RK_FORMAT_RGB_888) {
                dst_fmt = RK_FORMAT_BGR_888;  // RGB -> BGR
            } else {
                dst_fmt = src_fmt;  // No conversion needed
            }
            
            // Use C API function to avoid C++/C conflicts
            int ret = imcvtcolor_t(src_buf, dst_buf, src_fmt, dst_fmt, IM_COLOR_SPACE_DEFAULT, 1);
            
            if (ret != IM_STATUS_SUCCESS) {
                CVEDIX_WARN(cvedix_utils::string_format("[RGA] Color conversion failed: %s, falling back to OpenCV", 
                                                        imStrError((IM_STATUS)ret)));
                cv::cvtColor(src, dst, code);
                return 0;
            }
            
            return 0;
        }
        
        // For other color conversions, fallback to OpenCV
        // RGA has limited color space support
        CVEDIX_DEBUG(cvedix_utils::string_format("[RGA] Color conversion code %d not supported by RGA, using OpenCV", code));
        cv::cvtColor(src, dst, code);
        return 0;
    }

    int cvedix_rga_helper::crop_resize(const cv::Mat& src, cv::Mat& dst, const cv::Rect& crop_rect, const cv::Size& dst_size) {
        if (!is_available()) {
            // Fallback to OpenCV (crop then resize)
            cv::Mat cropped = src(crop_rect);
            cv::resize(cropped, dst, dst_size);
            return 0;
        }
        
        // Ensure destination Mat is allocated
        if (dst.empty() || dst.size() != dst_size || dst.type() != src.type()) {
            dst = cv::Mat(dst_size, src.type());
        }
        
        // RGA works directly with image data
        // wrapbuffer_virtualaddr is a macro that returns rga_buffer_t directly
        rga_buffer_t src_buf = wrapbuffer_virtualaddr(
            (void*)src.data, src.cols, src.rows, get_rga_format(src.type()));
        
        // Check if source buffer is valid
        if (src_buf.vir_addr == nullptr) {
            CVEDIX_WARN("[RGA] Failed to wrap source buffer for crop_resize, falling back to OpenCV");
            cv::Mat cropped = src(crop_rect);
            cv::resize(cropped, dst, dst_size);
            return 0;
        }
        
        // Set crop region in source buffer using im_rect
        im_rect src_rect;
        src_rect.x = crop_rect.x;
        src_rect.y = crop_rect.y;
        src_rect.width = crop_rect.width;
        src_rect.height = crop_rect.height;
        
        // Create intermediate buffer for cropped image
        cv::Mat cropped_temp(crop_rect.height, crop_rect.width, src.type());
        rga_buffer_t crop_buf = wrapbuffer_virtualaddr(
            (void*)cropped_temp.data, cropped_temp.cols, cropped_temp.rows, get_rga_format(cropped_temp.type()));
        
        // Create destination buffer
        rga_buffer_t dst_buf = wrapbuffer_virtualaddr(
            (void*)dst.data, dst.cols, dst.rows, get_rga_format(dst.type()));
        
        if (crop_buf.vir_addr == nullptr || dst_buf.vir_addr == nullptr) {
            CVEDIX_WARN("[RGA] Failed to create buffers for crop_resize, falling back to OpenCV");
            cv::Mat cropped = src(crop_rect);
            cv::resize(cropped, dst, dst_size);
            return 0;
        }
        
        // First, crop from source using C API
        int ret = imcrop_t(src_buf, crop_buf, src_rect, 1);
        
        if (ret == IM_STATUS_SUCCESS) {
            // Then resize the cropped image to destination size using C API
            ret = imresize_t(crop_buf, dst_buf, 0, 0, INTER_LINEAR, 1);
        }
        
        if (ret != IM_STATUS_SUCCESS) {
            CVEDIX_WARN(cvedix_utils::string_format("[RGA] Crop-resize failed: %s, falling back to OpenCV", 
                                                    imStrError((IM_STATUS)ret)));
            cv::Mat cropped = src(crop_rect);
            cv::resize(cropped, dst, dst_size);
            return 0;
        }
        
        return 0;
    }

    int cvedix_rga_helper::resize_letterbox(const cv::Mat& src, cv::Mat& dst, const cv::Size& dst_size, cv::Scalar pad_color) {
        // Calculate scale to maintain aspect ratio
        float scale = std::min((float)dst_size.width / src.cols, (float)dst_size.height / src.rows);
        int new_w = (int)(src.cols * scale);
        int new_h = (int)(src.rows * scale);
        
        int pad_x = (dst_size.width - new_w) / 2;
        int pad_y = (dst_size.height - new_h) / 2;
        
        // Allocate destination Mat
        dst = cv::Mat::zeros(dst_size, src.type());
        dst.setTo(pad_color);
        
        // Use RGA for resize if available, otherwise OpenCV
        cv::Mat resized;
        if (is_available()) {
            // Use RGA resize (hardware accelerated)
            resize(src, resized, cv::Size(new_w, new_h));
        } else {
            // Fallback to OpenCV
            cv::resize(src, resized, cv::Size(new_w, new_h));
        }
        
        // Copy resized image to center of destination (padding handled by setTo above)
        resized.copyTo(dst(cv::Rect(pad_x, pad_y, new_w, new_h)));
        
        return 0;
    }

} // namespace cvedix_utils

#endif // CVEDIX_WITH_RGA

