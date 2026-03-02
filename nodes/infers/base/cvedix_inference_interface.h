#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/core.hpp>

// Forward declaration if needed
namespace cvedix_nodes {

    /**
     * @brief Abstract interface for Hardware Inference Engines.
     * All backend plugins (CUDA, RKNN, CPU) must implement this interface.
     */
    class IInferenceEngine {
    public:
        virtual ~IInferenceEngine() = default;

        /**
         * @brief Initialize the inference engine with a model.
         * 
         * @param model_path Path to the model file (.rknn, .engine, .onnx)
         * @param config_path Optional path to configuration file
         * @return true if initialization is successful
         * @return false otherwise
         */
        virtual bool init(const std::string& model_path, const std::string& config_path = "") = 0;

        /**
         * @brief Perform inference on a batch of images.
         * 
         * @param inputs Vector of input images (cv::Mat)
         * @param outputs Vector of raw output tensors (cv::Mat)
         * @return true if inference is successful
         */
        virtual bool infer(const std::vector<cv::Mat>& inputs, std::vector<cv::Mat>& outputs) = 0;

        /**
         * @brief Get the Hardware Type name (e.g., "RKNN", "TensorRT", "CPU")
         */
        virtual std::string get_backend_name() const = 0;
    };

    /**
     * @brief Factory function signature for creating engine instances.
     * Plugin exports must implement a function with this signature named "create_engine".
     */
    typedef IInferenceEngine* (*CreateEngineFn)();
    
    /**
     * @brief Function signature to destroy engine instances created by the factory.
     * Ensures memory is allocated and deallocated in the same memory context.
     */
    typedef void (*DestroyEngineFn)(IInferenceEngine*);

}
