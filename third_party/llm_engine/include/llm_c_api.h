#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void* llm_handle;

/**
 * @brief Load an LLM model and return a handle.
 * 
 * @param model_path Path to the GGUF model file.
 * @param n_gpu_layers Number of layers to offload to GPU. -1 for all layers.
 * @return llm_handle The handle to the loaded engine, or NULL on failure.
 */
llm_handle llm_load(const char* model_path, int n_gpu_layers);

/**
 * @brief Generate text from a prompt.
 * 
 * @param handle The engine handle.
 * @param prompt The input prompt.
 * @param max_tokens Maximum number of tokens to generate.
 * @param temperature Temperature for sampling.
 * @return const char* Generated text. Must be freed with llm_free_string.
 */
const char* llm_infer(llm_handle handle, const char* prompt, int max_tokens, float temperature);

/**
 * @brief Free a string returned by llm_infer.
 */
void llm_free_string(const char* str);

/**
 * @brief Free the engine handle and unload the model.
 */
void llm_free(llm_handle handle);

#ifdef __cplusplus
}
#endif
