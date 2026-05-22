/**
 * @file llm_engine.h
 * @brief LLM Engine — Local LLM inference for CVEDIX SDK
 * 
 * C++ wrapper around llama.cpp providing a clean API for:
 * - GGUF model loading with GPU layer offloading
 * - Text generation with configurable sampling parameters
 * - Streaming token output via callbacks
 * - Thread-safe single-model inference
 * 
 * @section usage Usage
 * @code
 * llm::LLMConfig config;
 * config.model_path = "/path/to/model.gguf";
 * config.n_gpu_layers = -1;  // all on GPU
 * 
 * llm::LLMEngine engine;
 * engine.load_model(config);
 * 
 * std::string result = engine.generate("Analyze the scene: ...");
 * @endcode
 * 
 * @section thread_safety Thread Safety
 * The engine is NOT thread-safe. Callers must serialize access to generate().
 * This is by design — llama.cpp contexts are not thread-safe.
 * The CVEDIX pipeline node handles serialization internally.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>

namespace llm {

/**
 * @brief Configuration for LLM engine
 * 
 * All parameters have sensible defaults. At minimum, set model_path.
 */
struct LLMConfig {
    // ── Model Loading ──
    /// Path to GGUF model file (required)
    std::string model_path;
    
    /// Number of layers to offload to GPU. -1 = all layers (recommended)
    int n_gpu_layers = -1;
    
    /// Memory-map the model file for faster loading
    bool use_mmap = true;
    
    /// Lock model memory in RAM (prevents swapping, requires privileges)
    bool use_mlock = false;

    // ── Context ──
    /// Context window size in tokens (must be ≤ model's training context)
    int n_ctx = 4096;
    
    /// Batch size for prompt evaluation (higher = faster prompt processing, more VRAM)
    int n_batch = 512;
    
    /// Number of CPU threads for non-GPU operations
    int n_threads = 4;

    /// Flash Attention (highly recommended for Ampere+ GPUs)
    bool flash_attn = true;

    /// KV Cache Quantization type (e.g., 8 for Q8_0, 2 for FP16)
    /// Using int to avoid including ggml.h in the public header
    int type_k = 8; // GGML_TYPE_Q8_0
    int type_v = 8; // GGML_TYPE_Q8_0

    // ── Sampling ──
    /// Temperature for sampling (0.0 = greedy, higher = more creative)
    float temperature = 0.7f;
    
    /// Top-p (nucleus) sampling threshold
    float top_p = 0.9f;
    
    /// Top-k sampling: only consider top K tokens
    int top_k = 40;
    
    /// Repeat penalty to reduce repetition
    float repeat_penalty = 1.1f;
    
    /// Maximum tokens to generate per call
    int max_tokens = 512;

    // ── Prompting ──
    /// System prompt prepended to all generations
    std::string system_prompt = "";
};

/**
 * @brief Callback for streaming token output
 * 
 * Called for each generated token. Return false to stop generation early.
 * 
 * @param token The generated token text
 * @return true to continue, false to stop generation
 */
using TokenCallback = std::function<bool(const std::string& token)>;

/**
 * @brief Model information returned by get_model_info()
 */
struct ModelInfo {
    std::string name;              ///< Model name from GGUF metadata
    std::string architecture;      ///< Model architecture (e.g., "llama", "deepseek2")
    size_t param_count;            ///< Parameter count
    size_t vram_usage_mb;          ///< Estimated VRAM usage in MB
    int context_length;            ///< Maximum context length
    int vocab_size;                ///< Vocabulary size
};

/**
 * @brief Result of a text generation call with performance metrics
 */
struct GenerationResult {
    std::string text;              ///< Generated text
    bool success = false;          ///< Whether generation completed without error
    std::string error;             ///< Error message if success == false

    // ── Performance Metrics ──
    int prompt_tokens = 0;         ///< Number of tokens in the prompt
    int generated_tokens = 0;      ///< Number of tokens generated
    double prompt_eval_ms = 0.0;   ///< Time to evaluate prompt (ms)
    double generation_ms = 0.0;    ///< Time for token generation (ms)
    double total_ms = 0.0;         ///< Total inference time (ms)

    /// Tokens per second for generation (excluding prompt eval)
    double tokens_per_second() const {
        return generation_ms > 0.0 ? (generated_tokens * 1000.0 / generation_ms) : 0.0;
    }

    /// Time to first token in milliseconds
    double time_to_first_token_ms() const {
        return prompt_eval_ms;
    }
};

/**
 * @brief LLM inference engine
 * 
 * Provides local LLM inference using llama.cpp with CUDA acceleration.
 * 
 * Lifecycle:
 * 1. Construct engine
 * 2. Call load_model() with configuration
 * 3. Call generate() or generate_stream() for inference
 * 4. Destructor or unload_model() cleans up
 * 
 * @note NOT thread-safe. The pipeline node serializes access.
 */
class LLMEngine {
public:
    LLMEngine();
    ~LLMEngine();

    // Non-copyable, movable
    LLMEngine(const LLMEngine&) = delete;
    LLMEngine& operator=(const LLMEngine&) = delete;
    LLMEngine(LLMEngine&& other) noexcept;
    LLMEngine& operator=(LLMEngine&& other) noexcept;

    // ── Lifecycle ──
    
    /**
     * @brief Load a GGUF model with the given configuration
     * 
     * @param config Engine configuration (model_path is required)
     * @return true if model loaded successfully
     * 
     * @note Unloads any previously loaded model first
     * @note This is a heavy operation (may take 10-30s for large models)
     */
    bool load_model(const LLMConfig& config);
    
    /**
     * @brief Unload the current model and free all resources
     * 
     * Safe to call even if no model is loaded.
     */
    void unload_model();
    
    /**
     * @brief Check if a model is currently loaded
     */
    bool is_loaded() const;

    // ── Inference ──
    
    /**
     * @brief Generate text from a prompt (blocking)
     * 
     * @param prompt The input prompt text
     * @param override_config Optional config overrides for this call only
     *                        (temperature, max_tokens, etc.)
     * @return GenerationResult with text, success flag, and performance metrics
     * 
     * @note Blocks until generation completes or max_tokens is reached
     */
    GenerationResult generate(const std::string& prompt,
                         const LLMConfig* override_config = nullptr);
    
    /**
     * @brief Generate text with streaming token callback (blocking)
     * 
     * @param prompt The input prompt text
     * @param callback Called for each generated token. Return false to stop.
     * @param override_config Optional config overrides for this call only
     * @return GenerationResult with full text, metrics, or partial if callback stopped
     */
    GenerationResult generate_stream(const std::string& prompt,
                                TokenCallback callback,
                                const LLMConfig* override_config = nullptr);

    /**
     * @brief Enqueue a request for continuous batching (asynchronous)
     * 
     * @param prompt The input prompt text
     * @param on_token Called for each generated token. Return false to stop.
     * @param on_complete Called when generation finishes with the full text.
     * @param override_config Optional config overrides for this request
     * @return Request ID (can be used to cancel later, though cancellation isn't implemented yet)
     */
    int enqueue_request(const std::string& prompt,
                        TokenCallback on_token,
                        std::function<void(const std::string&, double, double)> on_complete,
                        const LLMConfig* override_config = nullptr);

    // ── Model Info ──
    
    /**
     * @brief Get information about the loaded model
     * @return ModelInfo struct, or default values if no model loaded
     */
    ModelInfo get_model_info() const;

    /**
     * @brief Get the last error message
     * @return Last error string, empty if no error
     */
    std::string last_error() const;

    /**
     * @brief Get estimated VRAM usage of the loaded model in MB
     * @return VRAM usage in MB, 0 if no model loaded
     */
    size_t get_vram_usage_mb() const;

private:
    /// PIMPL idiom — keeps llama.cpp headers out of this public header
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace llm
