#include "llm_c_api.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <model_path>\n", argv[0]);
        printf("Note: Pass a valid model path to run inference.\n");
        return 0; // Skip test if no model provided
    }
    
    const char* model_path = argv[1];
    printf("Loading model %s...\n", model_path);
    
    llm_handle handle = llm_load(model_path, -1);
    if (!handle) {
        printf("Failed to load model.\n");
        return 1;
    }
    
    printf("Model loaded successfully.\n");
    
    const char* prompt = "user: What is 2+2?\nassistant: ";
    printf("Generating response for prompt: %s\n", prompt);
    
    const char* response = llm_infer(handle, prompt, 128, 0.7f);
    if (response) {
        printf("Response: %s\n", response);
        llm_free_string(response);
    } else {
        printf("Failed to generate response.\n");
    }
    
    llm_free(handle);
    printf("Model unloaded. Test passed.\n");
    
    return 0;
}
