/**
 * @file cvedix_secondary_infer_node.h
 * @brief Base class for secondary inference nodes (ROI-based inference)
 * 
 * Secondary infer nodes perform inference on cropped regions (ROIs) from
 * primary detection results. This enables multi-stage pipelines.
 * 
 * @section secondary_flow Pipeline Flow
 * ```
 * Frame → Primary Infer → Detections → Secondary Infer (on each ROI) → Attributes
 * ```
 * 
 * @section secondary_filtering Target Filtering
 * Secondary inference can be filtered by:
 * - Primary class ID (p_class_ids_applied_to)
 * - Minimum target size (min_width_applied_to, min_height_applied_to)
 * 
 * @see cvedix_primary_infer_node Full-frame detection
 * @see cvedix_infer_node Base class
 */

#pragma once

#include "cvedix_infer_node.h"

namespace cvedix_nodes {

    /**
     * @brief Base class for secondary (ROI-based) inference nodes
     * 
     * Performs inference on cropped regions from primary detections.
     * Supports filtering by class ID and minimum size.
     * 
     * @see cvedix_infer_node Base class
     * @see cvedix_primary_infer_node Full-frame inference
     */
    class cvedix_secondary_infer_node: public cvedix_infer_node {
    private:
    protected:
        /**
         * @brief Prepare cropped regions for inference
         * @param frame_meta_with_batch Input frame metas with targets
         * @param[out] mats_to_infer Output cropped mats
         */
        virtual void prepare(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, std::vector<cv::Mat>& mats_to_infer) override;

        /**
         * @brief Check if target should be processed
         * @param primary_class_id Target's primary class ID
         * @param target_width Target width
         * @param target_height Target height
         * @return true if target passes filters
         */
        bool need_apply(int primary_class_id, int target_width, int target_height);

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param model_path Path to model file
         * @param model_config_path Optional config file path
         * @param labels_path Optional labels file path
         * @param input_width Model input width (default: 640)
         * @param input_height Model input height (default: 640)
         * @param batch_size Batch size (default: 1)
         * @param p_class_ids_applied_to Primary class IDs to process (empty = all)
         * @param min_width_applied_to Minimum target width (0 = no limit)
         * @param min_height_applied_to Minimum target height (0 = no limit)
         * @param crop_padding Padding around crop region (default: 10)
         * @param scale Pixel scale factor
         * @param mean Mean values for normalization
         * @param std Std values for normalization
         * @param swap_rb Swap R and B channels
         * @param swap_chn Swap channel order
         */
        cvedix_secondary_infer_node(std::string node_name, 
                            std::string model_path, 
                            std::string model_config_path = "", 
                            std::string labels_path = "", 
                            int input_width = 640, 
                            int input_height = 640, 
                            int batch_size = 1,
                            std::vector<int> p_class_ids_applied_to = std::vector<int>(),
                            int min_width_applied_to = 0,
                            int min_height_applied_to = 0,
                            int crop_padding = 10,
                            float scale = 1.0,
                            cv::Scalar mean = cv::Scalar(123.675, 116.28, 103.53),
                            cv::Scalar std = cv::Scalar(1),
                            bool swap_rb = true,
                            bool swap_chn = false);
        ~cvedix_secondary_infer_node();

        /// @brief Primary class IDs to apply secondary inference to (empty = all)
        std::vector<int> p_class_ids_applied_to;
        /// @brief Padding pixels around cropped region
        int crop_padding;
        /// @brief Minimum target height to process (0 = no limit)
        int min_height_applied_to = 0;
        /// @brief Minimum target width to process (0 = no limit)
        int min_width_applied_to = 0;
    };

}