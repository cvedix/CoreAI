#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vector>
#include <string>
#include "cvedix/utils/logger/cvedix_logger.h"

namespace cvedix_face_utils {

/**
 * @brief Face augmentation types
 */
enum class AugmentationType {
    ORIGINAL = 0,
    GLASSES,
    MASK,
    HAT
};

/**
 * @brief Result of augmentation containing image and type
 */
struct AugmentedFace {
    cv::Mat image;
    AugmentationType type;
    std::string type_name;
};

/**
 * @brief Face augmenter for overlaying glasses, masks, hats on aligned faces
 * 
 * Uses 5-point facial landmarks:
 *   [0] left eye, [1] right eye, [2] nose, [3] left mouth, [4] right mouth
 * 
 * Reference landmarks for 112x112 aligned face (InsightFace standard):
 *   left_eye: (38.29, 51.70), right_eye: (73.53, 51.50)
 *   nose: (56.03, 71.74), left_mouth: (41.55, 92.37), right_mouth: (70.73, 92.20)
 */
class FaceAugmenter {
public:
    /**
     * @brief Constructor
     * @param glasses_path Path to glasses PNG (with alpha channel)
     * @param mask_path Path to mask PNG (with alpha channel)
     * @param hat_path Path to hat PNG (with alpha channel)
     * @param face_size Expected aligned face size (default 112 for InsightFace)
     */
    FaceAugmenter(
        const std::string& glasses_path = "",
        const std::string& mask_path = "",
        const std::string& hat_path = "",
        int face_size = 112
    ) : face_size_(face_size) {
        
        // Load templates with alpha channel
        if (!glasses_path.empty()) {
            glasses_template_ = cv::imread(glasses_path, cv::IMREAD_UNCHANGED);
            if (glasses_template_.empty()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[FaceAugmenter] Failed to load glasses: %s", glasses_path.c_str()));
            } else {
                has_glasses_ = true;
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[FaceAugmenter] Loaded glasses template: %dx%d", 
                    glasses_template_.cols, glasses_template_.rows));
            }
        }
        
        if (!mask_path.empty()) {
            mask_template_ = cv::imread(mask_path, cv::IMREAD_UNCHANGED);
            if (mask_template_.empty()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[FaceAugmenter] Failed to load mask: %s", mask_path.c_str()));
            } else {
                has_mask_ = true;
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[FaceAugmenter] Loaded mask template: %dx%d",
                    mask_template_.cols, mask_template_.rows));
            }
        }
        
        if (!hat_path.empty()) {
            hat_template_ = cv::imread(hat_path, cv::IMREAD_UNCHANGED);
            if (hat_template_.empty()) {
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[FaceAugmenter] Failed to load hat: %s", hat_path.c_str()));
            } else {
                has_hat_ = true;
                CVEDIX_INFO(cvedix_utils::string_format(
                    "[FaceAugmenter] Loaded hat template: %dx%d",
                    hat_template_.cols, hat_template_.rows));
            }
        }
        
        // Set default reference landmarks for aligned 112x112 face
        ref_left_eye_ = cv::Point2f(38.29f, 51.70f);
        ref_right_eye_ = cv::Point2f(73.53f, 51.50f);
        ref_nose_ = cv::Point2f(56.03f, 71.74f);
        ref_left_mouth_ = cv::Point2f(41.55f, 92.37f);
        ref_right_mouth_ = cv::Point2f(70.73f, 92.20f);
    }
    
    /**
     * @brief Generate all augmented variants of a face
     * @param aligned_face Aligned face image (should be 112x112 or specified size)
     * @return Vector of augmented faces including original
     */
    std::vector<AugmentedFace> generate_all_variants(const cv::Mat& aligned_face) {
        std::vector<AugmentedFace> variants;
        
        // Original face
        AugmentedFace original;
        original.image = aligned_face.clone();
        original.type = AugmentationType::ORIGINAL;
        original.type_name = "original";
        variants.push_back(original);
        
        // Glasses variant
        if (has_glasses_) {
            AugmentedFace with_glasses;
            with_glasses.image = apply_glasses(aligned_face);
            with_glasses.type = AugmentationType::GLASSES;
            with_glasses.type_name = "glasses";
            variants.push_back(with_glasses);
        }
        
        // Mask variant
        if (has_mask_) {
            AugmentedFace with_mask;
            with_mask.image = apply_mask(aligned_face);
            with_mask.type = AugmentationType::MASK;
            with_mask.type_name = "mask";
            variants.push_back(with_mask);
        }
        
        // Hat variant
        if (has_hat_) {
            AugmentedFace with_hat;
            with_hat.image = apply_hat(aligned_face);
            with_hat.type = AugmentationType::HAT;
            with_hat.type_name = "hat";
            variants.push_back(with_hat);
        }
        
        return variants;
    }
    
    /**
     * @brief Apply glasses overlay to aligned face
     */
    cv::Mat apply_glasses(const cv::Mat& aligned_face) {
        if (!has_glasses_) {
            return aligned_face.clone();
        }
        
        cv::Mat result = aligned_face.clone();
        
        // Calculate eye center and width
        cv::Point2f eye_center((ref_left_eye_.x + ref_right_eye_.x) / 2.0f,
                               (ref_left_eye_.y + ref_right_eye_.y) / 2.0f);
        float eye_width = std::abs(ref_right_eye_.x - ref_left_eye_.x);
        
        // Scale glasses to fit eye width (glasses should be ~1.8x eye width)
        float target_width = eye_width * 1.8f;
        float scale = target_width / glasses_template_.cols;
        
        cv::Mat resized_glasses;
        cv::resize(glasses_template_, resized_glasses, cv::Size(), scale, scale, cv::INTER_LINEAR);
        
        // Position glasses centered on eyes
        int x = static_cast<int>(eye_center.x - resized_glasses.cols / 2.0f);
        int y = static_cast<int>(eye_center.y - resized_glasses.rows / 2.0f);
        
        overlay_with_alpha(result, resized_glasses, x, y);
        
        return result;
    }
    
    /**
     * @brief Apply mask overlay to aligned face (covers nose and mouth)
     */
    cv::Mat apply_mask(const cv::Mat& aligned_face) {
        if (!has_mask_) {
            return aligned_face.clone();
        }
        
        cv::Mat result = aligned_face.clone();
        
        // Mask should cover from nose tip to below mouth
        cv::Point2f mask_center((ref_left_mouth_.x + ref_right_mouth_.x) / 2.0f,
                                 (ref_nose_.y + ref_left_mouth_.y) / 2.0f);
        float face_width = std::abs(ref_right_eye_.x - ref_left_eye_.x) * 2.0f;
        
        // Scale mask
        float target_width = face_width;
        float scale = target_width / mask_template_.cols;
        
        cv::Mat resized_mask;
        cv::resize(mask_template_, resized_mask, cv::Size(), scale, scale, cv::INTER_LINEAR);
        
        // Position mask
        int x = static_cast<int>(mask_center.x - resized_mask.cols / 2.0f);
        int y = static_cast<int>(ref_nose_.y - resized_mask.rows * 0.2f);
        
        overlay_with_alpha(result, resized_mask, x, y);
        
        return result;
    }
    
    /**
     * @brief Apply hat overlay to aligned face (on top of head)
     */
    cv::Mat apply_hat(const cv::Mat& aligned_face) {
        if (!has_hat_) {
            return aligned_face.clone();
        }
        
        cv::Mat result = aligned_face.clone();
        
        // Hat should be above eyes, centered horizontally
        float face_width = std::abs(ref_right_eye_.x - ref_left_eye_.x) * 2.5f;
        float head_center_x = (ref_left_eye_.x + ref_right_eye_.x) / 2.0f;
        
        // Scale hat
        float target_width = face_width;
        float scale = target_width / hat_template_.cols;
        
        cv::Mat resized_hat;
        cv::resize(hat_template_, resized_hat, cv::Size(), scale, scale, cv::INTER_LINEAR);
        
        // Position hat above eyes
        int x = static_cast<int>(head_center_x - resized_hat.cols / 2.0f);
        int y = static_cast<int>(ref_left_eye_.y - resized_hat.rows * 0.9f);
        
        overlay_with_alpha(result, resized_hat, x, y);
        
        return result;
    }
    
    // Getters
    bool has_glasses() const { return has_glasses_; }
    bool has_mask() const { return has_mask_; }
    bool has_hat() const { return has_hat_; }
    int augmentation_count() const { 
        return 1 + (has_glasses_ ? 1 : 0) + (has_mask_ ? 1 : 0) + (has_hat_ ? 1 : 0); 
    }

private:
    /**
     * @brief Overlay image with alpha blending
     */
    void overlay_with_alpha(cv::Mat& background, const cv::Mat& overlay, int x, int y) {
        // Boundary check
        int bg_width = background.cols;
        int bg_height = background.rows;
        
        // Calculate visible region
        int x_start = std::max(0, x);
        int y_start = std::max(0, y);
        int x_end = std::min(bg_width, x + overlay.cols);
        int y_end = std::min(bg_height, y + overlay.rows);
        
        if (x_start >= x_end || y_start >= y_end) {
            return;  // No visible region
        }
        
        // Overlay region
        int ov_x_start = x_start - x;
        int ov_y_start = y_start - y;
        int roi_width = x_end - x_start;
        int roi_height = y_end - y_start;
        
        cv::Mat bg_roi = background(cv::Rect(x_start, y_start, roi_width, roi_height));
        cv::Mat ov_roi = overlay(cv::Rect(ov_x_start, ov_y_start, roi_width, roi_height));
        
        // Ensure background is 3 channel
        if (background.channels() == 1) {
            cv::cvtColor(background, background, cv::COLOR_GRAY2BGR);
            bg_roi = background(cv::Rect(x_start, y_start, roi_width, roi_height));
        }
        
        // Handle alpha blending
        if (overlay.channels() == 4) {
            // Split overlay into BGR and alpha
            std::vector<cv::Mat> channels;
            cv::split(ov_roi, channels);
            
            cv::Mat alpha = channels[3];
            cv::Mat overlay_bgr;
            cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, overlay_bgr);
            
            // Normalize alpha to [0, 1]
            cv::Mat alpha_f;
            alpha.convertTo(alpha_f, CV_32F, 1.0 / 255.0);
            
            // Blend each channel
            for (int c = 0; c < 3; c++) {
                for (int row = 0; row < roi_height; row++) {
                    for (int col = 0; col < roi_width; col++) {
                        float a = alpha_f.at<float>(row, col);
                        bg_roi.at<cv::Vec3b>(row, col)[c] = static_cast<uchar>(
                            a * overlay_bgr.at<cv::Vec3b>(row, col)[c] +
                            (1.0f - a) * bg_roi.at<cv::Vec3b>(row, col)[c]
                        );
                    }
                }
            }
        } else {
            // No alpha, just copy
            ov_roi.copyTo(bg_roi);
        }
    }
    
    // Templates
    cv::Mat glasses_template_;
    cv::Mat mask_template_;
    cv::Mat hat_template_;
    
    // Flags
    bool has_glasses_ = false;
    bool has_mask_ = false;
    bool has_hat_ = false;
    
    // Face size
    int face_size_;
    
    // Reference landmarks for aligned face
    cv::Point2f ref_left_eye_;
    cv::Point2f ref_right_eye_;
    cv::Point2f ref_nose_;
    cv::Point2f ref_left_mouth_;
    cv::Point2f ref_right_mouth_;
};

} // namespace cvedix_face_utils
