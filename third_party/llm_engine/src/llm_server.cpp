#include "llm_engine.h"
#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <ctime>

using json = nlohmann::json;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <model_path> [n_gpu_layers]" << std::endl;
        return 1;
    }
    
    std::string model_path = argv[1];
    int n_gpu_layers = -1;
    if (argc >= 3) {
        n_gpu_layers = std::stoi(argv[2]);
    }
    
    llm::LLMEngine engine;
    llm::LLMConfig config;
    config.model_path = model_path;
    config.n_gpu_layers = n_gpu_layers;
    
    std::cout << "Loading model " << model_path << " with " << n_gpu_layers << " GPU layers..." << std::endl;
    if (!engine.load_model(config)) {
        std::cerr << "Failed to load model" << std::endl;
        return 1;
    }

    auto info = engine.get_model_info();
    std::cout << "Model: " << info.name << " (" << info.architecture << ")" << std::endl;
    std::cout << "VRAM: ~" << info.vram_usage_mb << " MB" << std::endl;
    
    httplib::Server svr;

    // Health/info endpoint
    svr.Get("/health", [&engine](const httplib::Request&, httplib::Response& res) {
        auto info = engine.get_model_info();
        json health = {
            {"status", "ok"},
            {"model", info.name},
            {"architecture", info.architecture},
            {"vram_mb", info.vram_usage_mb},
            {"context_length", info.context_length}
        };
        res.set_content(health.dump(), "application/json");
    });
    
    svr.Post("/v1/chat/completions", [&engine](const httplib::Request& req, httplib::Response& res) {
        try {
            auto req_json = json::parse(req.body);
            
            std::string prompt = "";
            if (req_json.contains("messages") && req_json["messages"].is_array()) {
                for (const auto& msg : req_json["messages"]) {
                    if (msg.contains("role") && msg.contains("content")) {
                        prompt += msg["role"].get<std::string>() + ": " + msg["content"].get<std::string>() + "\n";
                    }
                }
                prompt += "assistant: ";
            } else if (req_json.contains("prompt")) {
                prompt = req_json["prompt"].get<std::string>();
            }
            
            llm::LLMConfig override_config;
            if (req_json.contains("temperature")) override_config.temperature = req_json["temperature"].get<float>();
            if (req_json.contains("max_tokens")) override_config.max_tokens = req_json["max_tokens"].get<int>();
            if (req_json.contains("top_p")) override_config.top_p = req_json["top_p"].get<float>();
            
            auto result = engine.generate(prompt, &override_config);

            if (!result.success) {
                json error = {{"error", result.error}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            json res_json = {
                {"id", "chatcmpl-123"},
                {"object", "chat.completion"},
                {"created", std::time(nullptr)},
                {"model", engine.get_model_info().name},
                {"choices", {{
                    {"index", 0},
                    {"message", {
                        {"role", "assistant"},
                        {"content", result.text}
                    }},
                    {"finish_reason", "stop"}
                }}},
                {"usage", {
                    {"prompt_tokens", result.prompt_tokens},
                    {"completion_tokens", result.generated_tokens},
                    {"total_tokens", result.prompt_tokens + result.generated_tokens}
                }},
                {"_performance", {
                    {"total_ms", result.total_ms},
                    {"tokens_per_second", result.tokens_per_second()}
                }}
            };
            
            res.set_content(res_json.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        }
    });
    
    std::cout << "Starting server on port 8001..." << std::endl;
    svr.listen("0.0.0.0", 8001);
    return 0;
}
