/**
 * @file llm_config.cpp
 * @brief LLM configuration loading and validation
 * 
 * Provides JSON-based configuration loading for LLMConfig
 * and parameter validation utilities.
 */

#include "llm_engine.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace llm {

// ─────────────────────────────────────────────
// Configuration Validation
// ─────────────────────────────────────────────

/**
 * @brief Validate and clamp configuration parameters to safe ranges
 * 
 * @param config Configuration to validate (modified in place)
 * @return true if config is valid (possibly after clamping)
 */
bool validate_config(LLMConfig& config) {
    bool valid = true;

    // Model path is required
    if (config.model_path.empty()) {
        std::cerr << "[LLM] Config error: model_path is empty" << std::endl;
        valid = false;
    }

    // Context size: clamp to reasonable range
    if (config.n_ctx < 64) {
        std::cerr << "[LLM] Config warning: n_ctx too small (" << config.n_ctx 
                  << "), clamping to 64" << std::endl;
        config.n_ctx = 64;
    }
    if (config.n_ctx > 131072) {
        std::cerr << "[LLM] Config warning: n_ctx very large (" << config.n_ctx 
                  << "), this may cause OOM. Clamping to 131072" << std::endl;
        config.n_ctx = 131072;
    }

    // Batch size: must be positive and ≤ context
    if (config.n_batch < 1) {
        config.n_batch = 1;
    }
    if (config.n_batch > config.n_ctx) {
        config.n_batch = config.n_ctx;
    }

    // Thread count: at least 1
    if (config.n_threads < 1) {
        config.n_threads = 1;
    }

    // Temperature: non-negative
    config.temperature = std::max(0.0f, config.temperature);

    // Top-p: clamp to [0.0, 1.0]
    config.top_p = std::clamp(config.top_p, 0.0f, 1.0f);

    // Top-k: at least 1 (or 0 to disable)
    if (config.top_k < 0) {
        config.top_k = 0;
    }

    // Repeat penalty: must be positive
    if (config.repeat_penalty <= 0.0f) {
        config.repeat_penalty = 1.0f;
    }

    // Max tokens: at least 1
    if (config.max_tokens < 1) {
        config.max_tokens = 1;
    }

    return valid;
}

/**
 * @brief Print configuration to stdout for debugging
 * 
 * @param config Configuration to print
 * @param prefix Optional prefix for each line
 */
void print_config(const LLMConfig& config, const std::string& prefix) {
    std::cout << prefix << "LLM Configuration:" << std::endl;
    std::cout << prefix << "  model_path:     " << config.model_path << std::endl;
    std::cout << prefix << "  n_gpu_layers:   " << config.n_gpu_layers 
              << (config.n_gpu_layers == -1 ? " (all)" : "") << std::endl;
    std::cout << prefix << "  n_ctx:          " << config.n_ctx << std::endl;
    std::cout << prefix << "  n_batch:        " << config.n_batch << std::endl;
    std::cout << prefix << "  n_threads:      " << config.n_threads << std::endl;
    std::cout << prefix << "  use_mmap:       " << (config.use_mmap ? "true" : "false") << std::endl;
    std::cout << prefix << "  use_mlock:      " << (config.use_mlock ? "true" : "false") << std::endl;
    std::cout << prefix << "  temperature:    " << config.temperature << std::endl;
    std::cout << prefix << "  top_p:          " << config.top_p << std::endl;
    std::cout << prefix << "  top_k:          " << config.top_k << std::endl;
    std::cout << prefix << "  repeat_penalty: " << config.repeat_penalty << std::endl;
    std::cout << prefix << "  max_tokens:     " << config.max_tokens << std::endl;
    if (!config.system_prompt.empty()) {
        std::cout << prefix << "  system_prompt:  \"" 
                  << config.system_prompt.substr(0, 80) 
                  << (config.system_prompt.length() > 80 ? "..." : "")
                  << "\"" << std::endl;
    }
}

} // namespace llm
