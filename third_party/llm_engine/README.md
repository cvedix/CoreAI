# LLM Engine

Đây là module C/C++ tích hợp `llama.cpp` làm inference backend cho hệ thống CVEDIX SDK. Nó cung cấp hiệu năng tối ưu trên GPU bằng cách sử dụng `GGML_CUDA=ON`, đặc biệt tối ưu cho phần cứng như **RTX 3080 20GB**.

## Cấu trúc module
- **C++ API**: `include/llm_engine.h`
- **C API**: `include/llm_c_api.h` - Wrapper C để dễ dàng gọi từ các ngôn ngữ khác (Python, Go, Rust...).
- **HTTP Server**: `llm_server` - Một HTTP Server tương thích với giao thức OpenAI `/v1/chat/completions`.
- **Unit Test**: `llm_c_api_test` - Kiểm thử cho C API.

## Hướng dẫn Build

Module này được biên dịch cùng với dự án chính của CVEDIX SDK. Đảm bảo bạn bật cờ `CVEDIX_WITH_LLM=ON` và `CVEDIX_WITH_CUDA=ON`.

```bash
mkdir -p build && cd build
cmake -DCVEDIX_WITH_LLM=ON -DCVEDIX_WITH_CUDA=ON ..
make -j$(nproc)
```

## Chạy C API Unit Test

```bash
./build/third_party/llm_engine/llm_c_api_test /path/to/deepseek-coder-v2-lite-16b-q8_0.gguf
```

## Khởi chạy HTTP Server (OpenAI-compatible)

Khởi động server trên cổng 8000. Bạn có thể chỉ định `n_gpu_layers` (ví dụ `-1` để offload 100% layers lên GPU).

```bash
./build/third_party/llm_engine/llm_server /path/to/deepseek-coder-v2-lite-16b-q8_0.gguf -1
```

Gửi request kiểm tra:

```bash
curl http://localhost:8000/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {"role": "user", "content": "Viết một hàm Python tính Fibonacci."}
    ],
    "temperature": 0.7,
    "max_tokens": 128
  }'
```

## Yêu cầu Hiệu năng (Acceptance Criteria)

- **Throughput**: Đạt tối thiểu **15 tokens/s** trên RTX 3080 20GB đối với model DeepSeek Coder V2 Lite 16B Q8_0.
- **Tối ưu Hóa**: Để đạt tốc độ cao nhất, cần offload toàn bộ layers (`n_gpu_layers = -1`), bật CUDA FP16, flash attention (được hỗ trợ trực tiếp bởi llama.cpp bên dưới).
