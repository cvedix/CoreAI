#include "llm_c_api.h"
#include "llm_engine.h"
#include <string.h>
#include <stdlib.h>
#include <string>

extern "C" {

llm_handle llm_load(const char* model_path, int n_gpu_layers) {
    if (!model_path) return nullptr;
    
    llm::LLMConfig config;
    config.model_path = model_path;
    config.n_gpu_layers = n_gpu_layers;
    
    llm::LLMEngine* engine = new llm::LLMEngine();
    if (!engine->load_model(config)) {
        delete engine;
        return nullptr;
    }
    
    return static_cast<llm_handle>(engine);
}

const char* llm_infer(llm_handle handle, const char* prompt, int max_tokens, float temperature) {
    if (!handle || !prompt) return nullptr;
    
    llm::LLMEngine* engine = static_cast<llm::LLMEngine*>(handle);
    
    llm::LLMConfig override_config;
    override_config.max_tokens = max_tokens;
    override_config.temperature = temperature;
    
    auto result = engine->generate(prompt, &override_config);
    if (!result.success) return nullptr;
    
    char* c_str = (char*)malloc(result.text.size() + 1);
    if (c_str) {
        strcpy(c_str, result.text.c_str());
    }
    return c_str;
}

void llm_free_string(const char* str) {
    if (str) {
        free((void*)str);
    }
}

void llm_free(llm_handle handle) {
    if (handle) {
        llm::LLMEngine* engine = static_cast<llm::LLMEngine*>(handle);
        engine->unload_model();
        delete engine;
    }
}

}
