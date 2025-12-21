/**
 * @file cvedix_infer_node.h
 * @brief Base class for all inference nodes in Core AI Runtime
 * 
 * This file defines the cvedix_infer_node base class for deep learning inference.
 * It implements a 4-step inference pipeline that derived classes can customize.
 * 
 * @section infer_pipeline Inference Pipeline
 * 1. **prepare()**: Extract images from frame_meta (pure virtual - must implement)
 * 2. **preprocess()**: Normalize, resize, create blob (default implementation provided)
 * 3. **infer()**: Run neural network forward pass (default implementation provided)
 * 4. **postprocess()**: Parse outputs and update frame_meta (pure virtual - must implement)
 * 
 * @section infer_types Inference Types
 * - **PRIMARY**: Runs on whole frame (detectors, pose estimators)
 * - **SECONDARY**: Runs on cropped regions (classifiers, feature extractors)
 * 
 * @section infer_backends Backends
 * Default implementation uses OpenCV DNN. Alternative backends:
 * - TensorRT (cvedix_trt_* nodes)
 * - ONNX Runtime (cvedix_*_ort_* nodes)
 * - Rockchip NPU (cvedix_rknn_* nodes)
 * 
 * @section infer_example Example Usage
 * @code
 * auto detector = std::make_shared<cvedix_yolo_detector_node>(
 *     "detector",
 *     "yolov5s.onnx",    // model path
 *     "",                 // config path
 *     "coco.names",       // labels path
 *     640, 640,           // input size
 *     1,                  // batch size
 *     0.5f                // confidence threshold
 * );
 * detector->attach_to({source_node});
 * @endcode
 * 
 * @see cvedix_primary_infer_node For primary inference
 * @see cvedix_secondary_infer_node For secondary inference
 */

#pragma once
#include <sstream>
#include <opencv2/dnn.hpp>
#include <opencv2/core.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {

    /**
     * @brief Specifies the inference mode based on input data source
     */
    enum cvedix_infer_type {
        PRIMARY,      ///< Infer on whole frame - detectors, pose estimators
        SECONDARY     ///< Infer on cropped regions - classifiers, feature extractors
    };

    /**
     * @brief Base class for all deep learning inference nodes
     * 
     * Provides a standardized 4-step inference pipeline and common infrastructure
     * for loading models, preprocessing inputs, and managing inference backends.
     * 
     * @section pipeline_steps Pipeline Steps
     * | Step | Method | Implementation | Description |
     * |------|--------|----------------|-------------|
     * | 1 | prepare() | Pure virtual | Extract input images from frame_meta |
     * | 2 | preprocess() | Default provided | Create blob, normalize, subtract mean |
     * | 3 | infer() | Default provided | Forward pass through neural network |
     * | 4 | postprocess() | Pure virtual | Parse outputs, update frame_meta |
     * 
     * @note This is an abstract base class. Use cvedix_primary_infer_node or
     *       cvedix_secondary_infer_node as intermediate bases, or implement
     *       a specific detector/classifier node.
     * 
     * @see cvedix_yolo_detector_node Example primary inference node
     * @see cvedix_classifier_node Example secondary inference node
     */
    class cvedix_infer_node: public cvedix_node {
    private:
        /// @brief Load class labels from file
        void load_labels();

    protected:
        /// @brief Inference type (PRIMARY or SECONDARY)
        cvedix_infer_type infer_type;
        /// @brief Path to model file (ONNX, Caffe, TensorFlow, etc.)
        std::string model_path;
        /// @brief Path to model config file (optional, for Caffe/TensorFlow)
        std::string model_config_path;
        /// @brief Path to labels/classes file
        std::string labels_path;
        /// @brief Model input width in pixels
        int input_width;
        /// @brief Model input height in pixels
        int input_height;
        /// @brief Batch size for inference
        int batch_size;
        /// @brief Mean values for normalization (default: ImageNet)
        cv::Scalar mean;
        /// @brief Standard deviation for normalization
        cv::Scalar std;
        /// @brief Scale factor for pixel values
        float scale;
        /// @brief Whether to swap Red and Blue channels (BGR↔RGB)
        bool swap_rb;

        /// @brief Whether to transpose channels (NCHW→NHWC)
        bool swap_chn;

        /**
         * @brief Protected constructor
         * 
         * @param node_name Unique node identifier
         * @param infer_type PRIMARY or SECONDARY inference mode
         * @param model_path Path to the neural network model file
         * @param model_config_path Path to config file (optional)
         * @param labels_path Path to class labels file (optional)
         * @param input_width Input image width (default: 128)
         * @param input_height Input image height (default: 128)
         * @param batch_size Batch size (default: 1)
         * @param scale Pixel value scale factor (default: 1.0)
         * @param mean Mean values for normalization (default: ImageNet means)
         * @param std Standard deviation for normalization (default: 1)
         * @param swap_rb Swap R and B channels (default: true)
         * @param swap_chn Transpose channels NCHW→NHWC (default: false)
         */
        cvedix_infer_node(std::string node_name, 
                    cvedix_infer_type infer_type, 
                    std::string model_path, 
                    std::string model_config_path = "", 
                    std::string labels_path = "", 
                    int input_width = 128, 
                    int input_height = 128, 
                    int batch_size = 1,
                    float scale = 1.0,
                    cv::Scalar mean = cv::Scalar(123.675, 116.28, 103.53),  // imagenet dataset
                    cv::Scalar std = cv::Scalar(1),
                    bool swap_rb = true,
                    bool swap_chn = false);
        
        /**
         * @brief Step 1: Prepare input images from frame meta
         * 
         * Pure virtual - must be implemented by derived classes.
         * Extracts regions of interest from frame_meta for inference.
         * 
         * @param frame_meta_with_batch Input frame metas
         * @param[out] mats_to_infer Output images to be processed
         */
        virtual void prepare(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch, std::vector<cv::Mat>& mats_to_infer) = 0;
        
        /**
         * @brief Step 2: Preprocess images for inference
         * 
         * Default implementation: resize, normalize, create 4D blob.
         * Can be overridden for custom preprocessing.
         * 
         * @param mats_to_infer Input images
         * @param[out] blob_to_infer Output blob for neural network
         */
        virtual void preprocess(const std::vector<cv::Mat>& mats_to_infer, cv::Mat& blob_to_infer);

        /**
         * @brief Step 3: Run neural network inference
         * 
         * Default implementation: forward pass through OpenCV DNN.
         * Override for alternative backends (TensorRT, ONNX Runtime, etc.)
         * 
         * @param blob_to_infer Input blob
         * @param[out] raw_outputs Network output tensors
         */
        virtual void infer(const cv::Mat& blob_to_infer, std::vector<cv::Mat>& raw_outputs);

        /**
         * @brief Step 4: Postprocess network outputs
         * 
         * Pure virtual - must be implemented by derived classes.
         * Parses raw network outputs and updates frame_meta with results.
         * 
         * @param raw_outputs Network output tensors
         * @param frame_meta_with_batch Frame metas to update with results
         */
        virtual void postprocess(const std::vector<cv::Mat>& raw_outputs, const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch) = 0;

        /**
         * @brief Log inference timing statistics
         * 
         * @param data_size Number of images processed
         * @param prepare_time Time for prepare step (ms)
         * @param preprocess_time Time for preprocess step (ms)
         * @param infer_time Time for inference step (ms)
         * @param postprocess_time Time for postprocess step (ms)
         */
        virtual void infer_combinations_time_cost(int data_size, int prepare_time, int preprocess_time, int infer_time, int postprocess_time);

        /**
         * @brief Execute the complete inference pipeline
         * 
         * Runs prepare→preprocess→infer→postprocess sequence.
         * Override to customize the pipeline flow.
         * 
         * @param frame_meta_with_batch Frame metas to process
         */
        virtual void run_infer_combinations(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& frame_meta_with_batch);

        /// @brief Loaded class labels
        std::vector<std::string> labels;
        
        /// @brief OpenCV DNN network backend
        cv::dnn::Net net;

        /**
         * @brief Handle single frame meta (calls batch version)
         * @note Marked final - override prepare/postprocess instead
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override final; 

        /**
         * @brief Handle batch of frame metas
         * @note Marked final - override prepare/postprocess instead
         */
        virtual void handle_frame_meta(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& meta_with_batch) override final; 

    public:
        /// @brief Destructor
        ~cvedix_infer_node();
    };
}