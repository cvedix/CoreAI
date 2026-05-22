/**
 * @file cvedix_llm_node.h
 * @brief Local LLM inference node using LLM engine
 * 
 * Pipeline node for running LLM inference locally on GPU using llama.cpp.
 * Processes detection results from upstream nodes and generates text descriptions.
 * 
 * Unlike the HTTP-based mllm_analyser_node, this node runs inference directly
 * on the local GPU — no Ollama/vLLM server required.
 * 
 * @section pipeline Pipeline Position
 * ```
 * source → detector → llm_node → osd → screen
 * ```
 * 
 * The node receives frame_meta with detection targets from upstream detector nodes,
 * formats them into a prompt, and generates a text analysis stored in 
 * frame_meta->description.
 * 
 * @section prompt_template Prompt Template
 * The prompt_template supports the following placeholders:
 * - `{detections}` — replaced with formatted detection results
 * - `{frame_index}` — replaced with current frame index
 * - `{num_targets}` — replaced with number of detected targets
 * 
 * @section prereq Prerequisites
 * - Compile with `-DCVEDIX_WITH_LLM=ON -DCVEDIX_WITH_CUDA=ON`
 * - GGUF model file accessible on disk
 * - Sufficient GPU VRAM for model + KV cache
 * 
 * @see cvedix_mllm_analyser_node For HTTP-based LLM inference (Ollama/OpenAI)
 */

#pragma once

#ifdef CVEDIX_WITH_LLM

#include "base/cvedix_primary_infer_node.h"
#include "cvedix/third_party/llm_engine/include/llm_engine.h"

#include <mutex>
#include <atomic>

namespace cvedix_nodes {

    /**
     * @brief Local LLM inference node using llama.cpp with CUDA acceleration
     * 
     * Generates text descriptions/analysis from detection results using a
     * locally loaded GGUF model. Inference runs directly on GPU.
     * 
     * @note This node serializes inference calls — only one generation
     *       runs at a time. This is by design (llama.cpp context is not thread-safe).
     * 
     * @note For vision-language models (processing raw frames), use the
     *       HTTP-based mllm_analyser_node with Ollama/vLLM instead.
     *       LLM currently supports text-only models.
     */
    class cvedix_llm_node : public cvedix_primary_infer_node {

    private:
        /// LLM engine instance (owns the llama.cpp model and context)
        std::unique_ptr<llm::LLMEngine> engine_;

        /// Engine configuration
        llm::LLMConfig config_;

        /// Prompt template with {detections}, {frame_index}, {num_targets} placeholders
        std::string prompt_template_;

        /// Mutex for serializing inference calls
        std::mutex infer_mutex_;

        /// Whether to skip frames if LLM is still processing previous frame
        bool skip_on_busy_;

        /// Flag indicating inference is in progress
        std::atomic<bool> busy_{false};

        /**
         * @brief Format detection targets into structured text for the LLM prompt
         * 
         * @param frame_meta Frame metadata containing detection results
         * @return Formatted string describing all detected targets
         */
        std::string format_detections(
            const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& frame_meta);

        /**
         * @brief Apply template substitutions to prompt_template
         * 
         * @param frame_meta Frame metadata for context
         * @return Final prompt string ready for LLM inference
         */
        std::string build_prompt(
            const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& frame_meta);

    protected:
        /**
         * @brief Override: run local LLM inference on detection results
         * 
         * Extracts targets from frame_meta, builds prompt, runs inference,
         * and stores result in frame_meta->description.
         */
        virtual void run_infer_combinations(
            const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& 
            frame_meta_with_batch) override;

        /**
         * @brief Override: no-op (LLM handles its own postprocessing)
         */
        virtual void postprocess(
            const std::vector<cv::Mat>& raw_outputs,
            const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& 
            frame_meta_with_batch) override;

    public:
        /**
         * @brief Construct LLM pipeline node
         * 
         * @param node_name Unique node identifier (e.g., "llm_0")
         * @param model_path Path to GGUF model file
         * @param prompt_template Prompt template with {detections} placeholder
         * @param n_gpu_layers Number of GPU layers (-1 = all, recommended)
         * @param n_ctx Context window size in tokens (default: 4096)
         * @param max_tokens Maximum tokens to generate per frame (default: 256)
         * @param temperature Sampling temperature (default: 0.3, low for factual)
         * @param skip_on_busy Skip frames when LLM is busy (default: true)
         * 
         * @throws std::runtime_error if model fails to load
         * 
         * @code
         * auto llm = std::make_shared<cvedix_llm_node>(
         *     "llm",
         *     "/models/deepseek-coder-v2-lite-16b-q8_0.gguf",
         *     "Analyze detections: {detections}. Summarize the scene.",
         *     -1, 4096, 256, 0.3f
         * );
         * llm->attach_to({detector});
         * @endcode
         */
        cvedix_llm_node(
            std::string node_name,
            std::string model_path,
            std::string prompt_template = "The following objects were detected in a surveillance camera frame:\n{detections}\n\nProvide a brief analysis of the scene.",
            int n_gpu_layers = -1,
            int n_ctx = 4096,
            int max_tokens = 256,
            float temperature = 0.3f,
            bool skip_on_busy = true
        );

        ~cvedix_llm_node();

        /**
         * @brief Get information about the loaded model
         * @return ModelInfo struct with model name, VRAM usage, etc.
         */
        llm::ModelInfo get_model_info() const;
    };

} // namespace cvedix_nodes

#endif // CVEDIX_WITH_LLM
