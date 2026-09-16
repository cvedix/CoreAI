/**
 * @file benchmark_llm.cpp
 * @brief Benchmark LLM: grid search of parameters and throughput / latency measuring.
 *
 * Runs tests varying:
 * - Flash Attention (true/false)
 * - Threads (2, 4, 8)
 * - KV Cache type (FP16 vs Q8_0)
 *
 * Saves output to a CSV file.
 */

#ifdef CVEDIX_WITH_LLM

#include "llm_engine.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <thread>

int main(int argc, char** argv) {
    // Resolution order: argv[1] -> CVEDIX_LLM_MODEL -> no default.
    // There is no sensible machine-independent default for a multi-GB .gguf, so
    // the benchmark refuses to run rather than pointing at a developer's homedir.
    std::string model_path;
    if (const char* env_model = std::getenv("CVEDIX_LLM_MODEL")) {
        model_path = env_model;
    }
    if (argc >= 2) {
        model_path = argv[1];
    }

    std::string csv_path = "llm_benchmark_report.csv";
    if (argc >= 3) {
        csv_path = argv[2];
    }

    if (model_path.empty()) {
        std::cerr << "Usage: benchmark_llm <model.gguf> [report.csv]\n"
                  << "       (or set CVEDIX_LLM_MODEL=<path to .gguf>)\n";
        return 1;
    }

    std::error_code ec;
    if (!std::filesystem::exists(model_path, ec)) {
        std::cerr << "Model not found: " << model_path << std::endl;
        return 1;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "LLM BENCHMARK MATRIX" << std::endl;
    std::cout << "Model: " << model_path << std::endl;
    std::cout << "CSV Output: " << csv_path << std::endl;
    std::cout << "==================================================" << std::endl;

    // Test matrix configurations
    std::vector<bool> flash_attn_opts = {true, false};
    std::vector<int> n_threads_opts = {2, 4, 8};
    std::vector<int> kv_type_opts = {1, 8}; // 1 = FP16 (GGML_TYPE_F16), 8 = Q8_0 (GGML_TYPE_Q8_0)

    std::ofstream csv_file(csv_path);
    if (!csv_file.is_open()) {
        std::cerr << "Failed to open CSV file: " << csv_path << std::endl;
        return 1;
    }

    // Write header
    csv_file << "Flash_Attention,Threads,KV_Cache_Type,VRAM_Usage_MB,Load_Time_MS,Prompt_Tokens,Generated_Tokens,TTFT_MS,Generation_Time_MS,Throughput_Tokens_Per_Sec,Total_Time_MS,Success,Error\n";

    std::string prompt = "Explain the theory of general relativity in two simple sentences.";

    for (bool flash_attn : flash_attn_opts) {
        for (int threads : n_threads_opts) {
            for (int kv_type : kv_type_opts) {
                std::string kv_name = (kv_type == 1) ? "FP16" : (kv_type == 8) ? "Q8_0" : "Unknown";
                std::cout << "Running Config: FlashAttn=" << (flash_attn ? "ON" : "OFF")
                          << ", Threads=" << threads
                          << ", KV=" << kv_name << "..." << std::endl;

                llm::LLMConfig config;
                config.model_path = model_path;
                config.n_gpu_layers = -1; // offload all to GPU
                config.flash_attn = flash_attn;
                config.n_threads = threads;
                config.type_k = kv_type;
                config.type_v = kv_type;
                config.max_tokens = 64;
                config.temperature = 0.0f; // deterministic for benchmark consistency

                auto start_load = std::chrono::steady_clock::now();
                llm::LLMEngine engine;
                bool load_success = engine.load_model(config);
                auto end_load = std::chrono::steady_clock::now();
                double load_time_ms = std::chrono::duration<double, std::milli>(end_load - start_load).count();

                if (!load_success) {
                    std::cerr << "  Failed to load model: " << engine.last_error() << std::endl;
                    csv_file << (flash_attn ? "true" : "false") << ","
                             << threads << ","
                             << kv_name << ","
                             << 0 << ","
                             << load_time_ms << ","
                             << 0 << ","
                             << 0 << ","
                             << 0.0 << ","
                             << 0.0 << ","
                             << 0.0 << ","
                             << 0.0 << ","
                             << "false" << ","
                             << "\"" << engine.last_error() << "\"\n";
                    csv_file.flush();
                    continue;
                }

                size_t vram_mb = engine.get_vram_usage_mb();
                
                // Warm-up run (to initialize CUDA graphs / memory pools / avoid cold start timing bias)
                engine.generate("Hello, how are you?", &config);

                // Benchmark run
                auto res = engine.generate(prompt, &config);

                std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << res.tokens_per_second() << " tok/s, "
                          << "TTFT: " << res.time_to_first_token_ms() << " ms, "
                          << "VRAM: " << vram_mb << " MB" << std::endl;

                csv_file << (flash_attn ? "true" : "false") << ","
                         << threads << ","
                         << kv_name << ","
                         << vram_mb << ","
                         << load_time_ms << ","
                         << res.prompt_tokens << ","
                         << res.generated_tokens << ","
                         << res.time_to_first_token_ms() << ","
                         << res.generation_ms << ","
                         << res.tokens_per_second() << ","
                         << res.total_ms << ","
                         << (res.success ? "true" : "false") << ","
                         << "\"" << res.error << "\"\n";
                csv_file.flush();

                engine.unload_model();
            }
        }
    }

    csv_file.close();
    std::cout << "==================================================" << std::endl;
    std::cout << "Benchmark complete. Report saved to: " << csv_path << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}

#else

#include <iostream>
int main() {
    std::cerr << "This benchmark requires LLM support." << std::endl;
    std::cerr << "Rebuild with: -DCVEDIX_WITH_LLM=ON" << std::endl;
    return 1;
}

#endif // CVEDIX_WITH_LLM
