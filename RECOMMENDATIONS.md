# CVEDIX AI Runtime SDK — Recommendations
## Cải tiến & Developer Experience

> **Lưu ý:** Đây là C/C++ SDK cho developers. Tất cả đề xuất tập trung vào developer experience (DX), code quality, và integration ease.

---

## Mục lục

1. [Tổng quan đánh giá](#1-tổng-quan-đánh-giá)
2. [Ưu điểm hiện tại](#2-ưu-điểm-hiện-tại)
3. [Ưu tiên cao (High Priority)](#3-ưu-tiên-cao)
4. [Ưu tiên trung bình (Medium Priority)](#4-ưu-tiên-trung-bình)
5. [Ưu tiên thấp (Low Priority)](#5-ưu-tiên-thấp)
6. [Roadmap khuyến nghị](#6-roadmap-khuyến-nghị)

---

## 1. Tổng quan đánh giá

### Điểm mạnh
- ✅ Kiến trúc plugin-based linh hoạt
- ✅ Đa backend inference (7 backends)
- ✅ 80+ nodes với đầy đủ tính năng AI
- ✅ Support nhiều hardware platforms
- ✅ Production-ready packaging (.deb, systemd)
- ✅ Dynamic pipeline engine

### Điểm yếu cần khắc phục
- ❌ Không có CI/CD, không có automated testing
- ❌ Nhiều samples bị commented/disabled, không có giải thích
- ❌ Documentation chưa đầy đủ (missing API reference)
- ❌ Không có Python/Rust bindings
- ❌ Build system phức tạp (CMake với nhiều conditional flags)
- ❌ Không có versioning policy rõ ràng
- ❌ Không có performance benchmark suite

---

## 2. Ưu điểm hiện tại

| Feature | Trạng thái | Ghi chú |
|---------|------------|---------|
| Plugin architecture | ✅ Có | Dynamic node pipeline engine |
| Multi-backend | ✅ 7 backends | ONNX, TensorRT, OpenVINO, RKNN, Paddle, llama.cpp, Milvus |
| Behavior Analysis | ✅ 22+ rules | Comprehensive traffic & safety rules |
| Face recognition | ✅ Multi-engine | InsightFace, SeetaFace6, FaceNet |
| Tracking | ✅ 5 algorithms | ByteTrack, SORT, OC-SORT, DeepSORT, BoTSORT |
| Output brokers | ✅ 13 brokers | MQTT, Kafka, UDP, SSE, Webhook, XML |
| Edge deployment | ✅ Ready | .deb, systemd |

---

## 3. Ưu tiên cao (High Priority)

### 3.1 CI/CD Pipeline & Automated Testing

**Vấn đề:** Không có CI/CD, mọi build/test đều làm tay → dễ lỗi, chậm release.

**Lưu ý quan trọng:** GitHub Actions standard runners **KHÔNG có NVIDIA GPU**. Do đó cần tách pipeline thành 2 phần:
- **CPU jobs**: Chạy trên GitHub Actions (ONNX Runtime CPU, base C++ code)
- **GPU jobs**: Chạy trên self-hosted runner có NVIDIA GPU (TensorRT, CUDA)

#### Kiến trúc CI/CD
```
┌─────────────────────────────────────────────────────────┐
│                    Push / PR                              │
└─────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
   ┌─────────┐       ┌─────────────┐   ┌─────────────┐
   │ CPU Build│       │  GPU Build  │   │    Docs     │
   │  (GH A)  │       │ (Self-host) │   │   (GH A)    │
   └─────────┘       └─────────────┘   └─────────────┘
        │                   │                   │
        ▼                   ▼                   ▼
   ┌─────────┐       ┌─────────────┐   ┌─────────────┐
   │  Unit    │       │  GPU Unit   │   │ Doxygen →   │
   │  Tests   │       │  Tests      │   │ GH Pages    │
   └─────────┘       └─────────────┘   └─────────────┘
        │                   │                   │
        ▼                   ▼                   ▼
   ┌─────────┐       ┌─────────────┐
   │  Build  │       │ Benchmark   │
   │  .deb   │       │ (GPU)       │
   └─────────┘       └─────────────┘
```

#### Files cần tạo
```
.github/workflows/
├── ci-cpu.yml           # CPU-only CI (GitHub Actions standard runners)
├── ci-gpu.yml           # GPU CI (self-hosted runner, manual/scheduled)
├── release.yml          # Release automation
└── codeql.yml           # Security scanning
```

#### ci-cpu.yml (GitHub Actions - CPU only)
```yaml
# .github/workflows/ci-cpu.yml
name: CVEDIX CI/CD (CPU)

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

env:
  CMAKE_BUILD_TYPE: Release

jobs:
  # ─── Lint ───
  lint:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install clang-format
        run: sudo apt-get install -y clang-format-14
      - name: Run clang-format check
        run: |
          find src -iname "*.hpp" -o -iname "*.cpp" | \
            xargs clang-format-14 --dry-run --Werror

  # ─── Build & Test (ONNX CPU) ───
  build-cpu:
    strategy:
      matrix:
        os: [ubuntu-22.04, ubuntu-24.04]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake git \
            libopencv-dev=4.* \
            libgstreamer1.0-dev \
            libgstreamer-plugins-base1.0-dev \
            libnlohmannjson-dev \
            libspdlog-dev \
            libgtest-dev \
            nlohmann-json3-dev

      - name: Configure CMake (ONNX CPU)
        run: |
          cmake -S . -B build \
            -DCMAKE_BUILD_TYPE=${{ env.CMAKE_BUILD_TYPE }} \
            -DCVEDIX_BACKEND=onnx \
            -DCVEDIX_BUILD_TESTS=ON \
            -DCVEDIX_BUILD_SAMPLES=OFF

      - name: Build
        run: cmake --build build -j$(nproc)

      - name: Run unit tests
        run: |
          cd build
          ctest --output-on-failure -R "(Unit|unit)"

      - name: Upload coverage
        uses: codecov/codecov-action@v3
        with:
          directory: build/coverage

  # ─── Build .deb ───
  build-deb:
    needs: build-cpu
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake git \
            libopencv-dev=4.* \
            libgstreamer1.0-dev \
            libnlohmannjson-dev \
            libspdlog-dev
      - name: Configure
        run: |
          cmake -S . -B build \
            -DCMAKE_BUILD_TYPE=Release \
            -DCVEDIX_BACKEND=onnx \
            -DCVEDIX_BUILD_TESTS=OFF
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Build .deb
        run: |
          cd build
          cpack -G DEB
      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: cvedix-sdk-deb-ubuntu22
          path: build/*.deb

  # ─── Docs ───
  docs:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Install Doxygen
        run: sudo apt-get install -y doxygen graphviz
      - name: Generate Doxygen
        run: cd docs && doxygen Doxyfile
      - name: Deploy to GitHub Pages
        uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./docs/html
        if: github.ref == 'refs/heads/main'
```

#### ci-gpu.yml (Self-hosted Runner có GPU)
```yaml
# .github/workflows/ci-gpu.yml
name: CVEDIX GPU CI

on:
  # Manual trigger only (requires self-hosted GPU runner)
  workflow_dispatch:
  # Or schedule: daily GPU tests
  schedule:
    - cron: '0 2 * * *'  # 2 AM UTC daily

# NOTE: Requires self-hosted runner with NVIDIA GPU
# Setup: https://github.com/cvedix/rapidmedia/actions/runners/new

env:
  CUDA_VERSION: "12.2.0"
  TENSORRT_VERSION: "8.6"

jobs:
  build-gpu:
    runs-on: [self-hosted, linux, gpu]
    steps:
      - uses: actions/checkout@v4

      - name: Build GPU Docker image
        run: |
          docker build \
            --target gpu-runtime \
            -t cvedix/sdk:gpu-dev \
            -f dockerfile .

      - name: Run GPU tests
        run: |
          docker run --rm --gpus all \
            cvedix/sdk:gpu-dev \
            /bin/bash -c "cd /workspace/build && ctest -R gpu --output-on-failure"

  benchmark-gpu:
    needs: build-gpu
    runs-on: [self-hosted, linux, gpu]
    steps:
      - uses: actions/checkout@v4
      - name: Run GPU benchmark
        run: |
          docker run --rm --gpus all \
            cvedix/sdk:gpu-dev \
            /bin/bash -c "./build/tools/benchmark/benchmark"
```

#### Self-Hosted GPU Runner Setup
```bash
#!/bin/bash
# Setup self-hosted GitHub Actions runner với NVIDIA GPU

REPO="cvedix/rapidmedia"
RUNNER_NAME="cvedix-gpu-01"

# 1. Install NVIDIA drivers
nvidia-smi  # Check driver đã install
# Nếu chưa có:
# sudo apt-get install -y nvidia-driver-535

# 2. Install Docker + NVIDIA Container Toolkit
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
curl -s -L https://nvidia.github.io/libnvidia-container/stable/rpm/nvidia-container-toolkit.repo | \
  sudo tee /etc/yum.repos.d/nvidia-container-toolkit.repo
sudo yum install -y nvidia-container-toolkit
sudo systemctl restart docker

# 3. Download GitHub Actions runner
cd /opt
curl -L -o runner.tar.gz \
  https://github.com/actions/runner/releases/latest/download/actions-runner-linux-x64.tar.gz
tar xzf runner.tar.gz

# 4. Configure runner với GPU label
./config \
  --url https://github.com/$REPO \
  --token <RUNNER_TOKEN> \
  --labels gpu --work _work

# 5. Chạy như service
sudo ./svc.sh install
sudo systemctl start github-runner
```

**Lợi ích:**
- CPU CI chạy tự động trên mỗi push/PR (GitHub free)
- GPU CI chạy manual/scheduled (cần self-hosted runner)
- Automated test coverage cho CPU code
- Tự động deploy docs lên GitHub Pages
- Build .deb artifact cho mỗi release

**Hardware requirement cho GPU runner:**
- NVIDIA GPU >= 8GB VRAM (T4, A10, RTX 3060+)
- CUDA 12.x + TensorRT 8.x
- Docker + NVIDIA Container Toolkit

**Estimated effort:** 1-2 tuần (CPU CI) + 1 ngày (setup GPU runner)

---

### 3.2 Unit Testing Framework

**Vấn đề:** Không có unit tests cho các node và backend.

**Giải pháp:** Thêm Google Test framework với cấu trúc tests rõ ràng

```
tests/
├── CMakeLists.txt                 # Test build configuration
├── unit/                          # Unit tests for individual components
│   ├── test_node_factory.cpp      # Test node creation
│   ├── test_tensorrt_backend.cpp  # Test TensorRT backend
│   ├── test_onnx_backend.cpp      # Test ONNX backend
│   ├── test_byte_track.cpp        # Test tracking algorithm
│   ├── test_yolo_detector.cpp     # Test detection node
│   └── test_ba_crossline.cpp      # Test behavior analysis
├── integration/                   # Pipeline integration tests
│   ├── test_simple_pipeline.cpp   # 2-node pipeline
│   ├── test_traffic_pipeline.cpp  # Full traffic detection
│   └── test_face_recognition.cpp  # Face recognition pipeline
├── fixtures/                      # Test data (models, videos)
│   ├── models/                    # Tiny test models
│   │   ├── tiny_yolo.onnx
│   │   └── tiny_facenet.onnx
│   └── videos/                    # Sample test videos
│       ├── sample_360p.mp4
│       └── sample_rtsp.txt        # RTSP test stream config
└── benchmark/                     # Performance benchmarks
    ├── benchmark_detection.cpp    # FPS benchmark
    ├── benchmark_tracking.cpp     # Tracking latency
    └── benchmark_backend.cpp      # Backend comparison
```

**Ví dụ unit test:**
```cpp
#include <gtest/gtest.h>
#include "nodes/yolo_detector.hpp"

TEST(YOLODetectorTest, BasicCreation) {
    auto detector = std::make_shared<YOLOv11DetectorNode>(
        0, "/tests/fixtures/models/tiny_yolo.onnx",
        cv::Size(320, 320), 0.5f, 0.4f
    );
    ASSERT_NE(detector, nullptr);
    EXPECT_EQ(detector->inputSize(), cv::Size(320, 320));
    EXPECT_EQ(detector->confidenceThreshold(), 0.5f);
}

TEST(YOLODetectorTest, ModelLoading) {
    auto detector = std::make_shared<YOLOv11DetectorNode>(
        0, "/tests/fixtures/models/tiny_yolo.onnx",
        cv::Size(320, 320), 0.5f, 0.4f
    );
    ASSERT_TRUE(detector->isLoaded());
    EXPECT_GT(detector->inputWidth(), 0);
    EXPECT_GT(detector->inputHeight(), 0);
}

TEST(YOLODetectorTest, InferenceOnSample) {
    auto detector = std::make_shared<YOLOv11DetectorNode>(
        0, "/tests/fixtures/models/tiny_yolo.onnx",
        cv::Size(320, 320), 0.3f, 0.3f
    );
    
    cv::Mat frame = cv::Mat::zeros(320, 320, CV_8UC3);
    auto detections = detector->process(0, frame, {});
    
    // Model tiny luôn predict at least 1 detection
    EXPECT_GE(static_cast<int>(detections.size()), 0);
}
```

**Estimated effort:** 2-3 tuần

---

### 3.3 Documentation Improvements

**Vấn đề:** Documentation thiếu và không đồng bộ.

**Giải pháp:**

| Tài liệu | Trạng thái | Hành động |
|----------|------------|-----------|
| README.md | ⚠️ Có nhưng cũ | Cập nhật quickstart guide (5 phút setup) |
| API Reference | ❌ Thiếu | Tự động sinh bằng Doxygen |
| Node docs | ⚠️ Từng phần | Chuẩn hóa format cho mỗi node |
| Sample docs | ❌ Thiếu | Tách samples thành standalone docs |
| Deployment guide | ❌ Thiếu | Tạo guide cho từng platform |
| Troubleshooting | ❌ Thiếu | FAQ & troubleshooting guide |

**Doxygen cấu hình:**
```cmake
# CMakeLists.txt - Add Doxygen generation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()
```

**Sample documentation format:**
```markdown
# YOLOv11 Detector

## Mô tả
Node phát hiện đối tượng sử dụng YOLOv11 model (ONNX hoặc TensorRT backend).

## Parameters
| Parameter | Type | Default | Mô tả |
|-----------|------|---------|-------|
| model_path | string | required | Đường dẫn file model (.onnx, .trt) |
| input_size | vector<int> | [640, 640] | Kích thước input |
| conf_threshold | float | 0.5 | Confidence threshold |
| nms_threshold | float | 0.4 | NMS threshold |

## Ví dụ
```cpp
auto detector = std::make_shared<YOLOv11DetectorNode>(
    0,                           // pipeline channel
    "/models/yolov11.onnx",      // model path
    cv::Size(640, 640),          // input size
    0.5f,                        // conf threshold
    0.4f                         // nms threshold
);
```

## Output Detections
```cpp
struct Detection {
    int class_id;
    float confidence;
    cv::Rect bbox;
};
```

## Backend Support
- ✅ CPU (ONNX Runtime)
- ✅ CUDA GPU (TensorRT)
- ⚠️ OpenVINO (WIP)
```

**Estimated effort:** 2-3 tuần

---

## 4. Ưu tiên trung bình (Medium Priority)

### 4.1 Python & Rust Bindings

**Vấn đề:** SDK chỉ có C++ API → Python developers không thể dùng trực tiếp.

**Giải pháp:** Thêm bindings thông qua pybind11 (đã có trong `3rdpart/pybind11/`)

**Python binding example:**
```python
# pip install cvedix-sdk
import cvedix

# Create pipeline trong Python
pipeline = cvedix.Pipeline("traffic_monitoring")

# Add nodes
camera = pipeline.add_source(cvedix.RTSPSrc(url="rtsp://192.168.1.100/stream"))
detector = pipeline.add_node(
    cvedix.YOLOv11Detector(
        model="/models/yolov11.onnx",
        input_size=(640, 640),
        conf_threshold=0.5
    )
)
tracker = pipeline.add_node(cvedix.ByteTrack(max_age=30))
mqtt = pipeline.add_output(
    cvedix.MQTTBroker(broker="tcp://192.168.1.1:1883", topic="traffic/events")
)

# Connect nodes
pipeline.connect(camera, detector)
pipeline.connect(detector, tracker)
pipeline.connect(tracker, mqtt)

# Run
pipeline.start()
```

**CMake:**
```cmake
# Create Python module
add_library(cvedix_py MODULE python_bindings.cpp)
target_link_libraries(cvedix_py PRIVATE cvedix_core pybind11::module)
pybind11_extend_module(cvedix_py)
```

**Rust binding example:**
```rust
use cvedix::{Pipeline, YOLOv11Detector, ByteTrack, MQTTBroker};

let mut pipeline = Pipeline::new("traffic_monitoring");
let detector = YOLOv11Detector::builder()
    .model("/models/yolov11.onnx")
    .input_size(640, 640)
    .conf_threshold(0.5)
    .build()?;
pipeline.add_node(detector);
pipeline.start();
```

**Estimated effort:** 3-4 tuần

---

### 4.2 Performance Benchmark Suite

**Vấn đề:** Không có cách đo lường hiệu năng để so sánh backends hoặc track regression.

**Giải pháp:** Built-in benchmark tool

```bash
# Benchmark detection across backends
./tools/benchmark --benchmark=detection \
    --model=/models/yolov11.onnx \
    --backends=onnx,tensorrt,openvino

# Output:
# Backend       | FPS   | Latency | GPU Mem
# --------------|-------|---------|--------
# ONNX (CPU)    | 12.3  | 81.3ms  | 256MB
# TensorRT      | 45.7  | 21.9ms  | 512MB
# OpenVINO      | 28.4  | 35.2ms  | 384MB
```

**CMakeLists.txt:**
```cmake
add_executable(benchmark tools/benchmark/main.cpp
                         tools/benchmark/detection_bench.cpp)
target_link_libraries(benchmark PRIVATE cvedix_core gtest_main)
```

**Estimated effort:** 1-2 tuần

---

### 4.3 Plugin Development SDK

**Vấn đề:** Tạo custom node mới cần copy-paste code từ existing nodes.

**Giải pháp:** Cung cấp plugin template và helper tools

```bash
# Generate a new plugin skeleton
./tools/plugin-generate MyCustomNode --base=Detector

# Output:
# my_custom_node/
# ├── CMakeLists.txt
# ├── my_custom_node.hpp
# ├── my_custom_node.cpp
# ├── test_my_custom_node.cpp
# └── README.md
```

**Plugin base class:**
```cpp
// Custom detector plugin
class MyCustomDetector : public cvedix::DetectorNode {
public:
    MyCustomDetector(int channel, const std::string& model_path)
        : DetectorNode(channel, model_path, cv::Size(640, 640)) {}

protected:
    std::vector<Detection> infer(const cv::Mat& frame) override {
        // Custom inference logic
    }
};

// Register plugin
CVEDIX_REGISTER_NODE(MyCustomDetector, "MyCustomDetector")
```

**Estimated effort:** 1-2 tuần

---

### 4.4 Versioning Policy & Semantic Versioning

**Vấn đề:** Không có versioning policy rõ ràng → khó upgrade, breaking changes không thông báo.

**Giải pháp:** Áp dụng Semantic Versioning (SemVer)

```cpp
// version.h
#define CVEDIX_VERSION_MAJOR 1
#define CVEDIX_VERSION_MINOR 2
#define CVEDIX_VERSION_PATCH 0
#define CVEDIX_VERSION_STRING "1.2.0"

// Runtime version check
bool cvedix_check_version(int major, int minor, int patch);
```

**BREAKING CHANGES guidelines:**
| Major version | Changes |
|---------------|---------|
| 0.x.x | Initial dev, API may change |
| 1.x.x | Stable API, bug fixes only |
| 2.x.x | Breaking changes, migration guide required |

**Changelog format:**
```markdown
## [1.2.0] - 2026-06-17

### Added
- ByteTrack tracking algorithm
- MQTT v5 broker support
- Python bindings

### Changed
- TensorRT backend uses TRT 10.x API

### Deprecated
- Old face engine API (use `setFaceEngine()` instead)

### Fixed
- Memory leak in RTSPSrc on connection error
```

**Estimated effort:** 3-5 ngày

---

### 4.5 CMake FindPackage Config

**Vấn đề:** Integration vào project khác khó (phải chỉ tay đường dẫn).

**Giải pháp:** Generate `cvedix-config.cmake` cho `find_package(cvedix)`

```cmake
# Consumer project CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(my_ai_app)

find_package(cvedix REQUIRED COMPONENTS tensorrt onnx)

add_executable(my_app main.cpp)
target_link_libraries(my_app cvedix::core)
```

**Generate in CVEDIX CMakeLists.txt:**
```cmake
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    cvedix-config-version.cmake
    VERSION ${CVEDIX_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    cmake/cvedix-config.cmake.in
    cvedix-config.cmake
    INSTALL_DESTINATION lib/cmake/cvedix
)
install(FILES cmake/cvedix-config.cmake
              cvedix-config-version.cmake
        DESTINATION lib/cmake/cvedix)
```

**Estimated effort:** 1 tuần

---

## 5. Ưu tiên thấp (Low Priority)

### 5.1 Cross-Platform Build Improvements

**Vấn đề:** Build system phức tạp với nhiều conditional flags.

**Giải pháp:** Simplify CMake với automatic platform detection

```cmake
# Simplified CMakeLists.txt
option(CVEDIX_BACKEND_CPU "Enable CPU backend" ON)
option(CVEDIX_BACKEND_TENSORRT "Enable TensorRT backend" OFF)
option(CVEDIX_BACKEND_OPENVINO "Enable OpenVINO backend" OFF)
option(CVEDIX_BUILD_TESTS "Build unit tests" OFF)
option(CVEDIX_BUILD_DOCS "Generate documentation" OFF)
option(CVEDIX_BUILD_PYTHON "Build Python bindings" OFF)

# Auto-detect GPU
if(CUDA_FOUND AND CVEDIX_BACKEND_TENSORRT)
    message(STATUS "TensorRT backend enabled")
    add_definitions(-DCVEDIX_BACKEND_TENSORRT)
    target_link_libraries(cvedix_core PRIVATE ${TENSORRT_LIBS})
endif()
```

**Estimated effort:** 1-2 tuần

---

### 5.2 Structured Logging

**Vấn đề:** Logging chưa consistent, khó debug trong production.

**Giải pháp:** JSON structured logging với spdlog

```cpp
// Logger setup
cvedix::Logger::setup({
    .level = cvedix::LogLevel::INFO,
    .format = cvedix::LogFormat::JSON,
    .output = cvedix::LogOutput::FILE | cvedix::LogOutput::STDOUT,
    .file = "/var/log/cvedix/sdk.log"
});

// Usage
CVEDIX_LOG_INFO("Pipeline started", "pipeline_id", "traffic_01", "fps", 30.0);
CVEDIX_LOG_WARN("High latency detected", "node", "detector", "latency_ms", 45.2);
CVEDIX_LOG_ERROR("Model load failed", "model", "/models/yolo.onnx", "error", e.what());
```

**Log output (JSON):**
```json
{
    "timestamp": "2026-06-17T19:00:00.000+07:00",
    "level": "WARN",
    "logger": "YOLODetector",
    "message": "High latency detected",
    "node": "detector",
    "latency_ms": 45.2
}
```

**Estimated effort:** 3-5 ngày

---

### 5.3 Hot-Reload Model

**Vấn đề:** Phải restart pipeline để đổi model.

**Giải pháp:** Support runtime model reload

```cpp
// Reload model without stopping pipeline
detector->reloadModel("/models/yolov11_v2.onnx");

// Callback khi model loaded
detector->onModelLoaded([](const std::string& path, float acc) {
    CVEDIX_LOG_INFO("Model loaded", "path", path, "accuracy", acc);
});
```

**Estimated effort:** 1 tuần

---

### 5.4 Configuration File Support

**Vấn đề:** Mọi cấu hình đều hardcoded trong C++.

**Giải pháp:** Load config từ YAML/JSON file

```yaml
# config/traffic_pipeline.yaml
pipeline:
  name: traffic_monitoring
  channels: 1

sources:
  - id: cam_01
    type: RTSPSrc
    url: rtsp://192.168.1.100:554/stream

nodes:
  - id: detector
    type: YOLOv11Detector
    model: /models/yolov11.onnx
    input_size: [640, 640]
    conf_threshold: 0.5
    nms_threshold: 0.4

  - id: tracker
    type: ByteTrack
    max_age: 30
    min_hits: 3

outputs:
  - id: mqtt
    type: MQTTBroker
    broker: tcp://192.168.1.1:1883
    topic: traffic/events
```

```cpp
// Load from file
auto config = cvedix::Config::load("config/traffic_pipeline.yaml");
auto pipeline = cvedix::PipelineFactory::create(config);
pipeline->start();
```

**Estimated effort:** 1-2 tuần

---

## 6. Roadmap khuyến nghị

### Phase 1: Foundation (Months 1-2)
| Task | Effort | Priority |
|------|--------|----------|
| CI/CD Pipeline | 2 tuần | 🔴 Critical |
| Unit Testing Framework | 3 tuần | 🔴 Critical |
| Versioning Policy | 3-5 ngày | 🟡 Important |
| Structured Logging | 3-5 ngày | 🟡 Important |

**Goal:** Có CI/CD, automated tests, và documentation basics

---

### Phase 2: Developer Experience (Months 2-4)
| Task | Effort | Priority |
|------|--------|----------|
| Documentation (Doxygen + samples) | 3 tuần | 🔴 Critical |
| CMake FindPackage | 1 tuần | 🟡 Important |
| Python Bindings | 3-4 tuần | 🟡 Important |
| Benchmark Suite | 1-2 tuần | 🟢 Nice to have |

**Goal:** Developers có thể dễ dàng integrate và dùng SDK

---

### Phase 3: Advanced Features (Months 4-6)
| Task | Effort | Priority |
|------|--------|----------|
| Plugin SDK | 1-2 tuần | 🟢 Nice to have |
| Hot-Reload Model | 1 tuần | 🟢 Nice to have |
| Config File Support | 1-2 tuần | 🟢 Nice to have |
| Cross-Platform Build | 1-2 tuần | 🟢 Nice to have |

**Goal:** SDK production-ready, dễ maintain và extend

---

## Appendix: Quick Wins (làm trong < 1 tuần)

| # | Improvement | Impact | Effort |
|---|-------------|--------|--------|
| 1 | Thêm .gitignore cho build artifacts | Medium | 30 phút |
| 2 | Tạo Makefile target `make test` | Medium | 2 giờ |
| 3 | Thêm version info vào binary | Low | 1 giờ |
| 4 | Tạo README cho mỗi sample | Medium | 4 giờ |
| 5 | Thêm structured logging (JSON) | Medium | 3 giờ |
| 6 | Tạo scripts tự động build cho từng platform | High | 6 giờ |
| 7 | Tạo CHANGELOG.md | Low | 1 giờ |
| 8 | Thêm Doxygen vào CMake | Medium | 2 giờ |