/**
 * @file cvedix_primary_infer_node.h
 * @brief Base class for primary inference nodes (full-frame detection)
 * 
 * Primary infer nodes perform inference on the entire frame to detect objects.
 * This is typically the first inference stage in a pipeline.
 * 
 * @section primary_vs_secondary Primary vs Secondary Inference
 * | Type | Input | Use Case |
 * |------|-------|----------|
 * | Primary | Full frame | Object detection, face detection |
 * | Secondary | Cropped regions | Classification, attribute extraction |
 * 
 * @see cvedix_secondary_infer_node For roi-based inference
 * @see cvedix_infer_node Base class
 */

#pragma once

#include "cvedix_infer_node.h"

namespace cvedix_nodes {

    /**
     * @brief Base class for primary (full-frame) inference nodes
     * 
     * Performs inference on the entire frame. Override run_infer()
     * to implement specific detection logic.
     * 
     * @note class_id_offset ensures unique IDs when multiple detectors exist
     * 
     * @see cvedix_infer_node Base class
     */
    class cvedix_primary_infer_node: public cvedix_infer_node {
    private:
    protected:
        /**
         * @brief Prepare full frames for inference
         * @param frame_meta_with_batch Input frame metas
         * @param[out] mats_to_infer Output mats (full frames)
         */
        virtual void prepare(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, std::vector<cv::Mat>& mats_to_infer) override;

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
         * @param class_id_offset Offset for class IDs (for multi-detector pipelines)
         * @param scale Pixel scale factor (default: 1.0)
         * @param mean Mean values for normalization
         * @param std Std values for normalization
         * @param swap_rb Swap R and B channels (default: true)
         * @param swap_chn Swap channel order (default: false)
         */
        cvedix_primary_infer_node(std::string node_name, 
                            std::string model_path, 
                            std::string model_config_path = "", 
                            std::string labels_path = "", 
                            int input_width = 640, 
                            int input_height = 640, 
                            int batch_size = 1,
                            int class_id_offset = 0,
                            float scale = 1.0,
                            cv::Scalar mean = cv::Scalar(123.675, 116.28, 103.53),
                            cv::Scalar std = cv::Scalar(1),
                            bool swap_rb = true,
                            bool swap_chn = false);
        ~cvedix_primary_infer_node();

        /// @brief Class ID offset to avoid conflicts with other detectors
        int class_id_offset;
    };
}