#include "llm_engine.h"

#include <llama.h>
#include <ggml.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <atomic>
#include <future>

namespace llm {

// ─────────────────────────────────────────────
// Helper functions for batch management
// ─────────────────────────────────────────────

static void batch_clear(llama_batch& batch) {
    batch.n_tokens = 0;
}

static void batch_add(
    llama_batch& batch,
    llama_token id,
    llama_pos pos,
    const std::vector<llama_seq_id>& seq_ids,
    bool logits)
{
    batch.token[batch.n_tokens] = id;
    batch.pos[batch.n_tokens] = pos;
    batch.n_seq_id[batch.n_tokens] = seq_ids.size();
    for (size_t i = 0; i < seq_ids.size(); ++i) {
        batch.seq_id[batch.n_tokens][i] = seq_ids[i];
    }
    batch.logits[batch.n_tokens] = logits;
    batch.n_tokens++;
}

// ─────────────────────────────────────────────
// PIMPL Implementation
// ─────────────────────────────────────────────

struct LLMEngine::Impl {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    LLMConfig active_config;
    std::atomic<bool> loaded{false};
    std::string last_error_;
    bool used_seq_ids[128] = {false};

    int allocate_seq_id() {
        for (int i = 0; i < 128; i++) {
            if (!used_seq_ids[i]) {
                used_seq_ids[i] = true;
                return i;
            }
        }
        return -1;
    }

    void free_seq_id(int id) {
        if (id >= 0 && id < 128) used_seq_ids[id] = false;
    }

    // ── Continuous Batching State ──
    struct Request {
        int id;
        std::string prompt;
        TokenCallback on_token;
        std::function<void(const std::string&, double, double)> on_complete;
        LLMConfig config;
        std::vector<llama_token> prompt_tokens;
    };

    struct Sequence {
        int id; // external request id
        int seq_id; // internal llama seq id
        Request req;
        std::string full_response;
        int n_pos = 0;
        int n_generated = 0;
        int n_prompt_evaluated = 0;
        bool is_finished = false;
        struct llama_sampler* smpl = nullptr;

        int batch_idx = -1;
        bool has_pending_token = false;
        llama_token pending_token = 0;

        // Timing metrics
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point first_token_time;
        double prompt_eval_ms = 0.0;
        double generation_ms = 0.0;
        bool first_token_recorded = false;

        ~Sequence() {
            if (smpl) {
                llama_sampler_free(smpl);
                smpl = nullptr;
            }
        }
    };

    std::queue<Request> pending_requests;
    std::map<int, std::unique_ptr<Sequence>> active_sequences;
    int next_req_id = 1;

    std::thread worker_thread;
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> stop_worker{false};

    ~Impl() {
        cleanup();
    }

    void cleanup() {
        stop_worker = true;
        cv.notify_all();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }

        std::lock_guard<std::mutex> lock(mtx);
        active_sequences.clear();
        for (int i=0; i<128; i++) used_seq_ids[i] = false;
        while(!pending_requests.empty()) pending_requests.pop();

        if (ctx) {
            llama_free(ctx);
            ctx = nullptr;
        }
        if (model) {
            llama_model_free(model);
            model = nullptr;
        }
        vocab = nullptr;
        loaded = false;
        stop_worker = false;
    }

    // ── Model Loading ──

    bool load(const LLMConfig& config) {
        cleanup();
        active_config = config;

        if (config.model_path.empty()) {
            std::cerr << "[LLM] Error: model_path is empty" << std::endl;
            return false;
        }

        llama_backend_init();

        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = config.n_gpu_layers;
        model_params.use_mmap = config.use_mmap;
        model_params.use_mlock = config.use_mlock;

        auto load_start = std::chrono::steady_clock::now();

        model = llama_model_load_from_file(config.model_path.c_str(), model_params);
        if (!model) {
            std::cerr << "[LLM] Error: Failed to load model from: " << config.model_path << std::endl;
            return false;
        }

        auto load_end = std::chrono::steady_clock::now();
        auto load_ms = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start).count();
        std::cout << "[LLM] Model loaded in " << load_ms << " ms" << std::endl;

        vocab = llama_model_get_vocab(model);
        if (!vocab) {
            std::cerr << "[LLM] Error: Failed to get vocabulary from model" << std::endl;
            cleanup();
            return false;
        }

        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = config.n_ctx;
        ctx_params.n_batch = config.n_batch;
        ctx_params.n_threads = config.n_threads;
        ctx_params.n_threads_batch = config.n_threads;
        ctx_params.n_seq_max = 128; // Required for continuous batching with multiple seq_ids
        ctx_params.flash_attn_type = config.flash_attn ? LLAMA_FLASH_ATTN_TYPE_ENABLED : LLAMA_FLASH_ATTN_TYPE_DISABLED;
        ctx_params.type_k = (ggml_type)config.type_k;
        ctx_params.type_v = (ggml_type)config.type_v;

        ctx = llama_init_from_model(model, ctx_params);
        if (!ctx) {
            std::cerr << "[LLM] Error: Failed to create llama context" << std::endl;
            cleanup();
            return false;
        }

        loaded = true;

        auto info = get_info();
        std::cout << "[LLM] Model: " << info.name << std::endl;
        std::cout << "[LLM] Architecture: " << info.architecture << std::endl;
        std::cout << "[LLM] Context: " << config.n_ctx << " tokens" << std::endl;
        std::cout << "[LLM] GPU layers: " << config.n_gpu_layers << (config.n_gpu_layers == -1 ? " (all)" : "") << std::endl;
        std::cout << "[LLM] KV Cache Quantization: K=" << config.type_k << ", V=" << config.type_v << std::endl;
        std::cout << "[LLM] Flash Attention: " << (config.flash_attn ? "ON" : "OFF") << std::endl;

        // Start worker thread
        stop_worker = false;
        worker_thread = std::thread(&Impl::worker_loop, this);

        return true;
    }

    // ── Tokenization ──

    std::vector<llama_token> tokenize(const std::string& text, bool add_special) {
        int n_tokens_max = text.length() + 128;
        std::vector<llama_token> tokens(n_tokens_max);

        int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(),
                                       tokens.data(), n_tokens_max,
                                       add_special, true);
        if (n_tokens < 0) {
            tokens.resize(-n_tokens);
            n_tokens = llama_tokenize(vocab, text.c_str(), text.length(),
                                       tokens.data(), tokens.size(),
                                       add_special, true);
            if (n_tokens < 0) return {};
        }

        tokens.resize(n_tokens);
        return tokens;
    }

    std::string token_to_piece(llama_token token) {
        char buf[256];
        int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
        if (n < 0) {
            std::vector<char> large_buf(-n);
            n = llama_token_to_piece(vocab, token, large_buf.data(), large_buf.size(), 0, true);
            if (n < 0) return "";
            return std::string(large_buf.data(), n);
        }
        return std::string(buf, n);
    }

    // ── Continuous Batching Worker ──

    llama_sampler* create_sampler(const LLMConfig& cfg) {
        auto* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
        if (!smpl) return nullptr;

        if (cfg.repeat_penalty != 1.0f) {
            llama_sampler_chain_add(smpl,
                llama_sampler_init_penalties(64, cfg.repeat_penalty, 0.0f, 0.0f));
        }
        if (cfg.top_k > 0) {
            llama_sampler_chain_add(smpl, llama_sampler_init_top_k(cfg.top_k));
        }
        if (cfg.top_p < 1.0f) {
            llama_sampler_chain_add(smpl, llama_sampler_init_top_p(cfg.top_p, 1));
        }
        if (cfg.temperature > 0.0f) {
            llama_sampler_chain_add(smpl, llama_sampler_init_temp(cfg.temperature));
        }
        llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

        return smpl;
    }

    int enqueue(const std::string& prompt, TokenCallback on_token, std::function<void(const std::string&, double, double)> on_complete, const LLMConfig& cfg) {
        if (!loaded) return -1;

        std::string full_prompt;
        const char* tmpl = llama_model_chat_template(model, nullptr);
        if (tmpl != nullptr) {
            std::vector<llama_chat_message> chat_msgs;
            if (!cfg.system_prompt.empty()) {
                chat_msgs.push_back({"system", cfg.system_prompt.c_str()});
            }
            chat_msgs.push_back({"user", prompt.c_str()});

            int32_t required_size = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, nullptr, 0);
            if (required_size > 0) {
                std::vector<char> buf(required_size + 1);
                int32_t actual_size = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, buf.data(), buf.size());
                if (actual_size >= 0) {
                    full_prompt = std::string(buf.data(), actual_size);
                }
            }
        }

        if (full_prompt.empty()) {
            full_prompt = cfg.system_prompt.empty() ? prompt : (cfg.system_prompt + "\n\n" + prompt);
        }

        auto prompt_tokens = tokenize(full_prompt, true);
        if (prompt_tokens.empty()) return -1;

        std::lock_guard<std::mutex> lock(mtx);
        int req_id = next_req_id++;
        Request req{req_id, prompt, on_token, on_complete, cfg, prompt_tokens};
        pending_requests.push(req);
        cv.notify_one();
        return req_id;
    }

    void worker_loop() {
        llama_batch batch = llama_batch_init(active_config.n_batch, 0, 128);

        while (!stop_worker) {
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this] { 
                    return stop_worker || !pending_requests.empty() || !active_sequences.empty(); 
                });

                if (stop_worker) break;

                while (!pending_requests.empty()) {
                    auto req = pending_requests.front();
                    pending_requests.pop();

                    int seq_id = allocate_seq_id();
                    if (seq_id < 0) {
                        std::cout << "[LLM] Queue full, waiting" << std::endl;
                        break;
                    }
                    std::cout << "[LLM] Starting sequence req_id=" << req.id << " seq_id=" << seq_id << std::endl;
                    auto seq = std::make_unique<Sequence>();
                    seq->id = req.id;
                    seq->seq_id = seq_id;
                    seq->req = req;
                    seq->smpl = create_sampler(req.config);
                    seq->start_time = std::chrono::steady_clock::now();
                    active_sequences[seq->id] = std::move(seq);
                }
            }

            batch_clear(batch);

            std::vector<int> active_ids;
            for (auto& pair : active_sequences) {
                if (!pair.second->is_finished) active_ids.push_back(pair.first);
            }

            if (active_ids.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            for (int id : active_ids) {
                auto& seq = active_sequences[id];
                seq->batch_idx = -1;

                if (seq->n_prompt_evaluated < (int)seq->req.prompt_tokens.size()) {
                    int n_eval = std::min((int)active_config.n_batch - batch.n_tokens, (int)seq->req.prompt_tokens.size() - seq->n_prompt_evaluated);
                    for (int i = 0; i < n_eval; i++) {
                        bool is_last = (seq->n_prompt_evaluated + i == (int)seq->req.prompt_tokens.size() - 1);
                        batch_add(batch, seq->req.prompt_tokens[seq->n_prompt_evaluated + i],
                                  seq->n_pos, {(llama_seq_id)seq->seq_id}, is_last);
                        seq->n_pos++;
                        if (is_last) seq->batch_idx = batch.n_tokens - 1;
                    }
                    seq->n_prompt_evaluated += n_eval;
                } else if (seq->has_pending_token) {
                    int n_ctx = llama_n_ctx(ctx);
                    if (seq->n_pos >= n_ctx - 1) {
                        int half_ctx = n_ctx / 2;
                        int p_size = seq->req.prompt_tokens.size();
                        int shift_amount = half_ctx;
                        llama_memory_seq_rm(llama_get_memory(ctx), seq->seq_id, p_size, p_size + shift_amount);
                        llama_memory_seq_add(llama_get_memory(ctx), seq->seq_id, p_size + shift_amount, seq->n_pos, -shift_amount);
                        seq->n_pos -= shift_amount;
                    }
                    
                    batch_add(batch, seq->pending_token, seq->n_pos, {(llama_seq_id)seq->seq_id}, true);
                    seq->n_pos++;
                    seq->batch_idx = batch.n_tokens - 1;
                    seq->has_pending_token = false;
                }
            }

            if (batch.n_tokens == 0) continue;

            if (llama_decode(ctx, batch) != 0) {
                std::cerr << "[LLM] Error: llama_decode failed" << std::endl;
                std::lock_guard<std::mutex> lock(mtx);
                active_sequences.clear();
                continue;
            }

            std::vector<int> finished_ids;
            for (int id : active_ids) {
                auto& seq = active_sequences[id];
                
                if (seq->batch_idx >= 0) {
                    llama_token new_token = llama_sampler_sample(seq->smpl, ctx, seq->batch_idx);
                    llama_sampler_accept(seq->smpl, new_token);

                    if (llama_vocab_is_eog(vocab, new_token) || seq->n_generated >= seq->req.config.max_tokens) {
                        seq->is_finished = true;
                    } else {
                        std::string piece = token_to_piece(new_token);
                        seq->full_response += piece;
                        seq->n_generated++;
                        seq->pending_token = new_token;
                        seq->has_pending_token = true;

                        // Record first token timing
                        if (!seq->first_token_recorded) {
                            seq->first_token_time = std::chrono::steady_clock::now();
                            seq->prompt_eval_ms = std::chrono::duration<double, std::milli>(
                                seq->first_token_time - seq->start_time).count();
                            seq->first_token_recorded = true;
                        }

                        if (seq->req.on_token && !seq->req.on_token(piece)) {
                            seq->is_finished = true;
                        }
                    }
                }

                if (seq->is_finished) {
                    finished_ids.push_back(seq->id);
                }
            }

            // Cleanup finished
            if (!finished_ids.empty()) {
                std::lock_guard<std::mutex> lock(mtx);
                for (int id : finished_ids) {
                    if (active_sequences.find(id) != active_sequences.end()) {
                        auto& seq = active_sequences[id];

                        // Calculate final timing
                        auto end_time = std::chrono::steady_clock::now();
                        double total_ms = std::chrono::duration<double, std::milli>(
                            end_time - seq->start_time).count();
                        seq->generation_ms = total_ms - seq->prompt_eval_ms;

                        // Log performance metrics
                        double tok_per_sec = seq->generation_ms > 0.0 ?
                            (seq->n_generated * 1000.0 / seq->generation_ms) : 0.0;
                        std::cout << "[LLM] Sequence req_id=" << seq->id
                                  << " done: " << seq->n_generated << " tokens"
                                  << ", prompt_eval=" << std::fixed << std::setprecision(1) << seq->prompt_eval_ms << "ms"
                                  << ", generation=" << seq->generation_ms << "ms"
                                  << ", total=" << total_ms << "ms"
                                  << ", speed=" << std::setprecision(1) << tok_per_sec << " tok/s"
                                  << std::endl;

                        if (seq->req.on_complete) {
                            seq->req.on_complete(seq->full_response, seq->prompt_eval_ms, seq->generation_ms);
                        }
                        llama_memory_seq_rm(llama_get_memory(ctx), seq->seq_id, -1, -1);
                        free_seq_id(seq->seq_id);
                        active_sequences.erase(id);
                    }
                }
            }
        }
        llama_batch_free(batch);
    }

    // ── Model Info ──

    ModelInfo get_info() const {
        ModelInfo info{};
        if (!ctx) return info;
        auto model = llama_get_model(ctx);

        char buf[256] = {};
        int n = llama_model_desc(model, buf, sizeof(buf));
        if (n > 0) info.name = std::string(buf, std::min(n, (int)sizeof(buf) - 1));

        info.param_count = llama_model_n_params(model);
        info.context_length = active_config.n_ctx;
        info.vocab_size = llama_vocab_n_tokens(vocab);
        info.vram_usage_mb = llama_model_size(model) / (1024 * 1024);

        // Detect architecture from model description
        // llama_model_desc returns format like "llama 7B Q4_K_M" or "deepseek2 16B Q8_0"
        char arch_buf[128] = {};
        int32_t arch_len = llama_model_meta_val_str(model, "general.architecture", arch_buf, sizeof(arch_buf));
        if (arch_len > 0) {
            info.architecture = std::string(arch_buf, arch_len);
        } else {
            std::string desc(buf, std::min(n, (int)sizeof(buf) - 1));
            auto space_pos = desc.find(' ');
            if (space_pos != std::string::npos) {
                info.architecture = desc.substr(0, space_pos);
            } else {
                info.architecture = desc.empty() ? "unknown" : desc;
            }
        }

        return info;
    }
};

// ─────────────────────────────────────────────
// Public API Implementation
// ─────────────────────────────────────────────

LLMEngine::LLMEngine() : pimpl_(std::make_unique<Impl>()) {}

LLMEngine::~LLMEngine() = default;

LLMEngine::LLMEngine(LLMEngine&& other) noexcept = default;
LLMEngine& LLMEngine::operator=(LLMEngine&& other) noexcept = default;

bool LLMEngine::load_model(const LLMConfig& config) {
    return pimpl_->load(config);
}

void LLMEngine::unload_model() {
    pimpl_->cleanup();
}

bool LLMEngine::is_loaded() const {
    return pimpl_->loaded;
}

int LLMEngine::enqueue_request(const std::string& prompt,
                               TokenCallback on_token,
                               std::function<void(const std::string&, double, double)> on_complete,
                               const LLMConfig* override_config) {
    const auto& cfg = override_config ? *override_config : pimpl_->active_config;
    return pimpl_->enqueue(prompt, on_token, on_complete, cfg);
}

GenerationResult LLMEngine::generate(const std::string& prompt,
                                     const LLMConfig* override_config) {
    GenerationResult result;
    if (!pimpl_->loaded) {
        result.error = "Model not loaded";
        pimpl_->last_error_ = result.error;
        return result;
    }

    auto total_start = std::chrono::steady_clock::now();
    
    struct InternalResult {
        std::string text;
        double prompt_eval_ms;
        double generation_ms;
    };
    std::promise<InternalResult> promise;
    auto future = promise.get_future();

    // Count prompt tokens for metrics
    const auto& cfg = override_config ? *override_config : pimpl_->active_config;
    
    std::string formatted_prompt;
    const char* tmpl = llama_model_chat_template(pimpl_->model, nullptr);
    if (tmpl != nullptr) {
        std::vector<llama_chat_message> chat_msgs;
        if (!cfg.system_prompt.empty()) {
            chat_msgs.push_back({"system", cfg.system_prompt.c_str()});
        }
        chat_msgs.push_back({"user", prompt.c_str()});

        int32_t required_size = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, nullptr, 0);
        if (required_size > 0) {
            std::vector<char> buf(required_size + 1);
            int32_t actual_size = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, buf.data(), buf.size());
            if (actual_size >= 0) {
                formatted_prompt = std::string(buf.data(), actual_size);
            }
        }
    }
    if (formatted_prompt.empty()) {
        formatted_prompt = cfg.system_prompt.empty() ? prompt : (cfg.system_prompt + "\n\n" + prompt);
    }
    auto prompt_tokens = pimpl_->tokenize(formatted_prompt, true);
    result.prompt_tokens = prompt_tokens.size();

    int generated_count = 0;
    enqueue_request(prompt,
        [&generated_count](const std::string& token) -> bool {
            generated_count++;
            return true;
        },
        [&promise](const std::string& res, double pe_ms, double gen_ms) {
            promise.set_value({res, pe_ms, gen_ms});
        }, override_config);

    auto internal_res = future.get();
    result.text = internal_res.text;
    result.prompt_eval_ms = internal_res.prompt_eval_ms;
    result.generation_ms = internal_res.generation_ms;
    result.generated_tokens = generated_count;
    result.success = true;

    auto total_end = std::chrono::steady_clock::now();
    result.total_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();

    // Log summary
    std::cout << "[LLM] generate(): " << result.prompt_tokens << " prompt tokens, "
              << result.generated_tokens << " generated, "
              << std::fixed << std::setprecision(1) << result.total_ms << " ms total, "
              << result.tokens_per_second() << " tok/s" << std::endl;

    return result;
}

GenerationResult LLMEngine::generate_stream(const std::string& prompt,
                                            TokenCallback callback,
                                            const LLMConfig* override_config) {
    GenerationResult result;
    if (!pimpl_->loaded) {
        result.error = "Model not loaded";
        pimpl_->last_error_ = result.error;
        return result;
    }

    auto total_start = std::chrono::steady_clock::now();
    
    struct InternalResult {
        std::string text;
        double prompt_eval_ms;
        double generation_ms;
    };
    std::promise<InternalResult> promise;
    auto future = promise.get_future();

    const auto& cfg = override_config ? *override_config : pimpl_->active_config;
    
    std::string formatted_prompt;
    const char* tmpl = llama_model_chat_template(pimpl_->model, nullptr);
    if (tmpl != nullptr) {
        std::vector<llama_chat_message> chat_msgs;
        if (!cfg.system_prompt.empty()) {
            chat_msgs.push_back({"system", cfg.system_prompt.c_str()});
        }
        chat_msgs.push_back({"user", prompt.c_str()});

        int32_t required_size = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, nullptr, 0);
        if (required_size > 0) {
            std::vector<char> buf(required_size + 1);
            int32_t actual_size = llama_chat_apply_template(tmpl, chat_msgs.data(), chat_msgs.size(), true, buf.data(), buf.size());
            if (actual_size >= 0) {
                formatted_prompt = std::string(buf.data(), actual_size);
            }
        }
    }
    if (formatted_prompt.empty()) {
        formatted_prompt = cfg.system_prompt.empty() ? prompt : (cfg.system_prompt + "\n\n" + prompt);
    }
    auto prompt_tokens = pimpl_->tokenize(formatted_prompt, true);
    result.prompt_tokens = prompt_tokens.size();

    int generated_count = 0;
    enqueue_request(prompt,
        [&callback, &generated_count](const std::string& token) -> bool {
            generated_count++;
            if (callback) return callback(token);
            return true;
        },
        [&promise](const std::string& res, double pe_ms, double gen_ms) {
            promise.set_value({res, pe_ms, gen_ms});
        }, override_config);

    auto internal_res = future.get();
    result.text = internal_res.text;
    result.prompt_eval_ms = internal_res.prompt_eval_ms;
    result.generation_ms = internal_res.generation_ms;
    result.generated_tokens = generated_count;
    result.success = true;

    auto total_end = std::chrono::steady_clock::now();
    result.total_ms = std::chrono::duration<double, std::milli>(total_end - total_start).count();

    return result;
}

ModelInfo LLMEngine::get_model_info() const {
    return pimpl_->get_info();
}

std::string LLMEngine::last_error() const {
    return pimpl_->last_error_;
}

size_t LLMEngine::get_vram_usage_mb() const {
    if (pimpl_->loaded) {
        return pimpl_->get_info().vram_usage_mb;
    }
    return 0;
}

} // namespace llm
