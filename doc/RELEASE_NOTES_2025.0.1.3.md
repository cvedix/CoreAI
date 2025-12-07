# CVEDIX AI Runtime SDK - Release Notes v2025.0.1.3

**Release Date:** December 7, 2025  
**Version:** 2025.0.1.3

---

## 🎉 Tổng quan

Phiên bản 2025.0.1.2.1 của CVEDIX AI Runtime SDK mang đến hai tính năng mới quan trọng: **InsightFace Face Recognition** với ONNX models và **Multimodal LLM (MLLM) Analysis Node** cho phân tích ảnh thông minh. Ngoài ra, phiên bản này cũng bao gồm nhiều cải tiến về platform compatibility và bug fixes.

---

## ✨ Tính năng mới

### 1. InsightFace Face Recognition Node (ONNX-based)

- **Node mới:** `cvedix_insight_face_recognition_node`
- **Mô tả:** Node nhận diện khuôn mặt sử dụng InsightFace models (ArcFace, CosFace, etc.) trực tiếp từ ONNX files
- **Backend:** OpenCV DNN (không cần TensorRT)
- **Tính năng:**
  - Hỗ trợ face alignment tự động với 5-point landmarks
  - Tự động phát hiện embedding dimension từ model output
  - L2 normalization cho embeddings
  - Tương thích với các InsightFace models từ [deepinsight/insightface](https://github.com/deepinsight/insightface)
  - Hỗ trợ CUDA acceleration (nếu có)

**Ví dụ sử dụng:**
```cpp
#include "cvedix/nodes/infers/cvedix_insight_face_recognition_node.h"

// Tạo node với ONNX model
auto recognizer = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>(
    "recognizer",
    "./models/face/arcface_r50.onnx",  // ONNX model path
    112,    // input width (standard for InsightFace)
    112,    // input height
    true    // enable_alignment = true (use 5-point landmarks)
);

// Sử dụng trong pipeline
yunet_detector->attach_to({rtsp_src});
recognizer->attach_to({yunet_detector});
osd->attach_to({recognizer});
```

**Samples:**
- `insightface_sample.cpp` - Basic recognition pipeline
- `insightface_register_recognize_face_sample.cpp` - Register và recognize faces với database

**Lợi ích:**
- Không cần TensorRT, chạy được trên mọi platform (AMD64, ARM64)
- Dễ dàng tích hợp với các InsightFace models có sẵn
- Performance tốt với OpenCV DNN backend
- Hỗ trợ CUDA nếu có GPU

---

### 2. Multimodal LLM (MLLM) Analysis Node

- **Node mới:** `cvedix_mllm_analyser_node`
- **Mô tả:** Node phân tích ảnh/video frames sử dụng Multimodal Large Language Models
- **Backend:** Hỗ trợ Ollama và OpenAI API
- **Tính năng:**
  - Phân tích nội dung ảnh/video frames bằng natural language
  - Hỗ trợ custom prompts
  - Tích hợp với OSD node để hiển thị kết quả
  - Hỗ trợ nhiều LLM backends (Ollama, OpenAI)
  - Tự động thêm description vào frame_meta

**Ví dụ sử dụng:**
```cpp
#include "cvedix/nodes/infers/cvedix_mllm_analyser_node.h"

// Tạo node với Ollama backend
auto mllm_analyser = std::make_shared<cvedix_nodes::cvedix_mllm_analyser_node>(
    "mllm_analyser_0",
    "minicpm-v:8b",                    // model name
    "Mô tả nội dung ảnh trong 1 câu",  // prompt
    "http://localhost:11434",          // Ollama API URL
    "",                                 // API key (không cần cho Ollama)
    llmlib::LLMBackendType::Ollama     // backend type
);

// Sử dụng trong pipeline
mllm_analyser->attach_to({image_src});
mllm_osd->attach_to({mllm_analyser});
screen_des->attach_to({mllm_osd});
```

**Samples:**
- `mllm_analyse_sample.cpp` - Phân tích ảnh với Ollama
- `mllm_analyse_sample_openai.cpp` - Phân tích ảnh với OpenAI API

**Use Cases:**
- Mô tả nội dung ảnh/video
- Phân tích cảnh quan, thời tiết
- Đọc và hiểu text trong ảnh (OCR + understanding)
- Tạo câu chuyện từ ảnh
- Phân tích hành vi, tình huống

---

## 🔧 Cải tiến

### Platform Compatibility

#### RTSP Source Node - Platform-aware Decoder Selection
- **Cải thiện:** Logic tự động chọn decoder phù hợp với platform
  - **ARM64 (Rockchip):** Tự động sử dụng `mppvideodec` (hardware decoder) nếu có
  - **AMD64/x86_64:** Luôn sử dụng `avdec_h264` (software decoder)
  - **Fallback:** Tự động fallback về software decoder nếu hardware decoder không khả dụng
- **Impact:** RTSP streams hoạt động đúng trên cả Rockchip và AMD64 platforms

**Trước khi sửa:**
```
❌ Lỗi trên AMD64: mppvideodec không tồn tại
```

**Sau khi sửa:**
```
✅ Tự động chọn decoder phù hợp với platform
✅ Log rõ ràng về decoder được sử dụng
```

### Build System

#### OpenSSL Dependency Check
- **Cải thiện:** Kiểm tra OpenSSL khi `CVEDIX_WITH_LLM=ON`
  - Thông báo lỗi rõ ràng với hướng dẫn cài đặt
  - Hỗ trợ nhiều Linux distributions (Ubuntu/Debian, Fedora/RHEL, Arch Linux)
- **Impact:** Dễ dàng debug và cài đặt dependencies

#### Kafka Support
- **Cải thiện:** Conditional compilation cho Kafka
  - Kafka headers chỉ được include khi `CVEDIX_WITH_KAFKA=ON`
  - Tự động loại bỏ Kafka files khỏi build khi không enable
  - Kiểm tra `librdkafka` với thông báo lỗi rõ ràng
- **Impact:** Build thành công ngay cả khi không có Kafka dependencies

### Logging

#### Auto-cleanup Old Log Files
- **Tính năng mới:** Tự động xóa log files cũ khi tạo log file mới theo ngày
- **Impact:** Giảm dung lượng disk, dễ quản lý logs

---

## 🐛 Bug Fixes

### Bug 1: RTSP Source - Platform Incompatibility
- **Vấn đề:** `mppvideodec` được sử dụng trên mọi platform, gây lỗi trên AMD64
- **Fix:** Thêm logic kiểm tra platform và chỉ sử dụng `mppvideodec` trên ARM64 (Rockchip)
- **Impact:** RTSP streams hoạt động đúng trên cả Rockchip và AMD64

### Bug 2: Kafka Headers Included Unconditionally
- **Vấn đề:** Kafka headers được include ngay cả khi `CVEDIX_WITH_KAFKA=OFF`, gây lỗi compile
- **Fix:** Wrap Kafka includes và member variables trong `#ifdef CVEDIX_WITH_KAFKA`
- **Impact:** Build thành công khi không có Kafka dependencies

### Bug 3: OpenSSL Not Found Error
- **Vấn đề:** CMake error không rõ ràng khi OpenSSL không tìm thấy
- **Fix:** Thêm kiểm tra và thông báo lỗi chi tiết với hướng dẫn cài đặt
- **Impact:** Dễ dàng debug và fix dependency issues

### Bug 4: Image Source Node - Missing Preprocessor Guards
- **Vấn đề:** `#endif` không có `#ifdef` tương ứng trong `cvedix_image_src_node`
- **Fix:** Thêm `#ifdef CVEDIX_WITH_GSTREAMER` guards
- **Impact:** Compile đúng khi GStreamer disabled

---

## 📦 Package Contents

Package bao gồm:

### Libraries
- `libcvedix_instance_sdk.so` - Main SDK library
- CMake config files trong `/opt/cvedix/lib/cmake/cvedix/`
- pkg-config file trong `/opt/cvedix/lib/pkgconfig/`

### Headers
- Tất cả header files trong `/opt/cvedix/include/cvedix/`
- **Mới:** `nodes/infers/cvedix_insight_face_recognition_node.h`
- **Mới:** `nodes/infers/cvedix_mllm_analyser_node.h` (khi `CVEDIX_WITH_LLM=ON`)
- Bao gồm:
  - Nodes (src, des, mid, infer, track, osd, broker)
  - Objects (frame_meta, targets, etc.)
  - Utils (logger, mqtt_client, analysis_board, etc.)

### Samples
- **Mới:** `insightface_sample` - Basic InsightFace recognition
- **Mới:** `insightface_register_recognize_face_sample` - Register và recognize faces
- **Mới:** `mllm_analyse_sample` - LLM analysis với Ollama
- **Mới:** `mllm_analyse_sample_openai` - LLM analysis với OpenAI
- Basic samples (1-1-1, 1-N-N, ba_crossline, etc.)
- RKNN samples (rknn_detector_sample, etc.)
- TensorRT samples (nếu build với `CVEDIX_WITH_TRT=ON`)

### Data
- `cvedix_data/` directory với models và configs (nếu có)
- Được cài đặt vào `/opt/cvedix/bin/cvedix_data/`

---

## 📋 Dependencies

### Bắt buộc
- OpenCV >= 4.6 (với DNN module)
- GStreamer >= 1.14.5
  - gstreamer1.0-plugins-base
  - gstreamer1.0-plugins-good
  - gstreamer1.0-plugins-bad
  - libgstreamer1.0-0
  - libgstreamer-plugins-base1.0-0

### Tùy chọn
- **LLM Support:** OpenSSL (libssl-dev) - **Bắt buộc nếu `CVEDIX_WITH_LLM=ON`**
  - Ubuntu/Debian: `sudo apt-get install libssl-dev`
  - Fedora/RHEL: `sudo dnf install openssl-devel`
  - Arch Linux: `sudo pacman -S openssl`
- **Kafka:** librdkafka-dev - **Bắt buộc nếu `CVEDIX_WITH_KAFKA=ON`**
  - Ubuntu/Debian: `sudo apt-get install librdkafka-dev`
  - Fedora/RHEL: `sudo dnf install librdkafka-devel`
  - Arch Linux: `sudo pacman -S librdkafka`
- **RKNN:** rknn-toolkit2 (cho Rockchip NPU)
- **MQTT:** libmosquitto-dev (cho MQTT support)
- **TensorRT:** TensorRT libraries (cho NVIDIA GPU)
- **CUDA:** CUDA toolkit (cho GPU acceleration)
- **PaddlePaddle:** PaddlePaddle libraries (cho Paddle inference)

---

## 🚀 Installation

### Cài đặt từ Debian Package

Package được đóng gói riêng cho từng architecture:

```bash
# Cài đặt package cho ARM64
sudo dpkg -i cvedix-ai-runtime-2025.0.1.3-arm64.deb

# Cài đặt package cho x86_64
sudo dpkg -i cvedix-ai-runtime-2025.0.1.3-x86_64.deb

# Hoặc sử dụng apt
sudo apt-get install ./cvedix-ai-runtime-2025.0.1.3-x86_64.deb

# Fix dependencies nếu cần
sudo apt-get install -f
```

**Lưu ý:** Chọn đúng package cho architecture của hệ thống:
- **ARM64** (aarch64): `cvedix-ai-runtime-2025.0.1.3-arm64.deb`
- **x86_64**: `cvedix-ai-runtime-2025.0.1.3-x86_64.deb`

### Build từ Source

```bash
# Clone repository
git clone <repository-url>
cd core_ai_runtime

# Build với InsightFace và LLM support
mkdir build && cd build
cmake .. \
    -DCVEDIX_WITH_LLM=ON \
    -DCVEDIX_WITH_KAFKA=OFF \
    -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Build Debian package
cd ..
./debian/build_deb.sh --release
```

### Sử dụng SDK

```cmake
# Trong CMakeLists.txt của project
find_package(cvedix REQUIRED)
target_link_libraries(your_target cvedix::cvedix_instance_sdk)
```

---

## 📝 Migration Notes

### Từ version 2025.0.1.2

1. **InsightFace Recognition:**
   - Node mới, không ảnh hưởng code hiện tại
   - Có thể thay thế TensorRT InsightFace node nếu không cần TensorRT optimization
   - Tương thích với các InsightFace models có sẵn

2. **LLM Analysis:**
   - Node mới, yêu cầu `CVEDIX_WITH_LLM=ON` khi build
   - Cần OpenSSL development packages
   - Cần Ollama server hoặc OpenAI API key để sử dụng

3. **RTSP Source:**
   - Không cần thay đổi code
   - Tự động chọn decoder phù hợp với platform
   - Log rõ ràng hơn về decoder được sử dụng

4. **Kafka Support:**
   - Nếu không sử dụng Kafka, có thể tắt với `CVEDIX_WITH_KAFKA=OFF`
   - Build sẽ nhanh hơn và không cần librdkafka

---

## 🔍 Known Issues

- Không có known issues trong phiên bản này

---

## 📚 Documentation

- **API Documentation:** Xem header files trong `/opt/cvedix/include/cvedix/`
- **Samples:** Xem source code trong `/opt/cvedix/bin/` hoặc `samples/` directory
- **InsightFace Recognition:** Xem `samples/insightface_sample.cpp`
- **LLM Analysis:** Xem `samples/mllm_analyse_sample.cpp`
- **Packaging Guide:** Xem `README_PACKAGING.md`

---

## 👥 Contributors

CVEDIX Development Team

---

## 📞 Support

- **Email:** support@cvedix.com
- **Website:** https://www.cvedix.com

---

## 📄 License

Proprietary - CVEDIX

---

**Changelog:**
- v2025.0.1.3 (2025-12-07)
  - ✨ Added InsightFace Face Recognition Node (ONNX-based)
  - ✨ Added Multimodal LLM (MLLM) Analysis Node
  - 🔧 Improved RTSP source node with platform-aware decoder selection
  - 🔧 Enhanced OpenSSL dependency check with clear error messages
  - 🔧 Improved Kafka support with conditional compilation
  - 🔧 Added auto-cleanup for old log files
  - 🐛 Fixed RTSP source platform incompatibility (AMD64 support)
  - 🐛 Fixed Kafka headers being included unconditionally
  - 🐛 Fixed OpenSSL not found error messages
  - 🐛 Fixed image source node missing preprocessor guards

