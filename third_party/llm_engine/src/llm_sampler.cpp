/**
 * @file llm_sampler.cpp
 * @brief Sampler utility functions for LLM
 * 
 * Provides helper functions for creating and configuring
 * llama.cpp sampler chains from LLMConfig parameters.
 * 
 * The sampler chain applies filters in order:
 * 1. Repeat penalty (reduces repetitive output)
 * 2. Top-K (keeps only K most likely tokens)
 * 3. Top-P / nucleus (keeps tokens summing to probability P)
 * 4. Temperature (controls randomness)
 * 5. Distribution sampler (final token selection)
 */

#include "llm_engine.h"
#include <llama.h>
#include <iostream>

namespace llm {

// ─────────────────────────────────────────────
// Sampler Preset Configurations
// ─────────────────────────────────────────────

/**
 * Preset configurations for common use cases.
 * These can be used as starting points for LLMConfig.
 */

LLMConfig make_deterministic_config() {
    LLMConfig cfg;
    cfg.temperature = 0.0f;   // Greedy decoding
    cfg.top_k = 1;
    cfg.top_p = 1.0f;
    cfg.repeat_penalty = 1.0f;
    cfg.max_tokens = 256;
    return cfg;
}

LLMConfig make_creative_config() {
    LLMConfig cfg;
    cfg.temperature = 0.9f;
    cfg.top_k = 50;
    cfg.top_p = 0.95f;
    cfg.repeat_penalty = 1.15f;
    cfg.max_tokens = 1024;
    return cfg;
}

LLMConfig make_factual_config() {
    LLMConfig cfg;
    cfg.temperature = 0.3f;
    cfg.top_k = 20;
    cfg.top_p = 0.85f;
    cfg.repeat_penalty = 1.1f;
    cfg.max_tokens = 512;
    return cfg;
}

LLMConfig make_scene_analysis_config() {
    LLMConfig cfg;
    cfg.temperature = 0.2f;   // Low temperature for consistent scene descriptions
    cfg.top_k = 10;
    cfg.top_p = 0.8f;
    cfg.repeat_penalty = 1.15f;
    cfg.max_tokens = 256;     // Short, focused descriptions
    cfg.system_prompt = "You are a surveillance camera scene analyzer. "
                        "Provide concise, factual descriptions of detected objects and events. "
                        "Focus on safety-relevant observations.";
    return cfg;
}

} // namespace llm
