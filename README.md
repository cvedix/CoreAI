# Core Runtime — AI Video Analytics SDK

> **⚠️ TÀI LIỆU NỘI BỘ — KHÔNG CHIA SẺ RA NGOÀI**
>
> Repo này chứa toàn bộ source code SDK Core Runtime (EdgeOS SDK / AI Core Runtime).
> Mọi thông tin trong tài liệu này chỉ dành cho team phát triển nội bộ CVEDIX.

---

## Tổng quan

Core Runtime là framework C++17 plugin-based để xây dựng pipeline xử lý video real-time. Mỗi đơn vị xử lý gọi là **Node** — là một plugin độc lập, có thể kết hợp tự do để tạo pipeline từ đơn giản (detect object) đến phức tạp (multi-channel traffic violation + behavior analysis + face recognition + LPR + mLLM).

### Tính năng chính

- **Multi-backend inference**: OpenCV DNN, TensorRT, RKNN, ONNX Runtime, PaddleInference, mLLM (Ollama/vLLM/OpenAI)
- **Multi-platform**: x86_64 + ARM64 (Jetson, RK3588, Ascend 310/910)
- **Dynamic pipeline**: Attach/detach node lúc runtime, không cần restart
- **80+ nodes**, 12 categories, kèm samples minh họa topology & VLM
- **Deployment**: `.deb` package, Docker, systemd service

---

## Bắt đầu nhanh

### 1. Cài dependencies

```bash
# Non-interactive (base deps)
make setup-auto

# Interactive (chọn thêm hardware-specific deps)
make setup
```

### 2. Build

```bash
# Auto-detect hardware
make build

# Hoặc chọn target cụ thể
make build-cpu                      # CPU only
make build-nvidia-openvino-ort      # NVIDIA + OpenVINO + ONNX Runtime
make build-rockchip                 # Rockchip RK35xx (RKNN + RGA)
```

### 3. Chạy thử

```bash
./build/bin/1-1-1_sample
./build/bin/N-N_sample
```

> 📖 Dev mới: bắt đầu với [**docs/ONBOARDING.md**](./docs/ONBOARDING.md)
> 📖 Chi tiết build options & CMake flags: xem [**docs/DEVELOPMENT.md**](./docs/DEVELOPMENT.md)

### Nhận diện PPE từ video bằng TensorRT

```bash
bash scripts/run_ppe_video.sh
```

Đọc `data/videos/16.22.09.mp4`, chạy `data/models/yolov11n_ppe_detection_fp16.engine`
trên NVIDIA GPU và xuất video có nhận diện cùng CSV vào `output/ppe/`.
Build cần `-DCVEDIX_WITH_TRT=ON`; chọn `--backend onnx` để chạy model ONNX trên CPU.
Xem [hướng dẫn PPE](./docs/PPE_VIDEO.md) để build, đổi tham số và chạy thử một đoạn.

Thêm `--mqtt-host` để đẩy event người không an toàn (ảnh crop, bbox, `track_id`,
`event_id`) lên nền tảng HeraMind qua MQTT. Asset đăng ký device type, transform
và dashboard nằm trong [integrations/heramind_ppe](./integrations/heramind_ppe/README.md).

---

## Cấu trúc thư mục

```
core/
├── nodes/          # Tất cả nodes: source, inference, tracking, BA, OSD, broker, destination
├── objects/        # Frame metadata, control/event data structures
├── utils/          # Logger, analysis board, helpers, clients
├── capi/           # C API layer (libcvedix_capi) — nền cho bindings Java/C#/C
├── sdk/            # Submodule → github.com/cvedix/SDK (gói phân phối cho người dùng)
├── samples/        # Chương trình mẫu (topology + VLM)
├── integrations/   # Asset tích hợp nền tảng ngoài (HeraMind: device-type, transform, dashboard)
├── excepts/        # Custom exceptions
├── third_party/    # Backend dependencies (TensorRT, PaddleOCR, ONNX Runtime, RKNN...)
├── scripts/        # Build, setup, package scripts
├── docs/           # Tài liệu kỹ thuật — bắt đầu từ docs/README.md
│   ├── guides/     #   Hướng dẫn tính năng (BA, VLM, Face)
│   ├── ci/         #   Tài liệu CI/CD
│   ├── reports/    #   Báo cáo benchmark
│   └── internal/   #   Tài liệu lịch sử/tham khảo
├── sdk-docs/       # Submodule → github.com/cvedix/sdk-docs (website sdk.cvedix.com)
├── cmake/          # CMake modules
├── deb_package/    # Packaging configs
├── benchmarks/     # Benchmark programs & reports
├── configs/        # Config mẫu (Milvus, RTSP cameras)
├── assets/         # Hình ảnh cho docs
└── tools/          # Công cụ hỗ trợ
```

---

## Node Catalog (tóm tắt)

| Category | Số lượng | Ví dụ |
|----------|----------|-------|
| **Source** | 6 | File, RTSP, RTMP, UDP, Image, Application |
| **Inference** | 35+ | YOLO (v3/v8/v11), YuNet, InsightFace, SFace, FaceNet, PaddleOCR, OpenPose, mLLM, CLIP, Real-ESRGAN |
| **Tracking** | 5 | SORT, ByteTrack, OC-SORT, DeepSORT, BoTSORT |
| **Behavior Analysis** | 22+ rules | Crossline, wrong-way, red-light, speed, crowding, loitering, area enter/exit, lane & parking violation |
| **OSD** | 18 | Detection, Face, Plate, Pose, Segmentation, BA overlay, mLLM description |
| **Broker** | 13 | MQTT, Kafka, SSE, UDP Socket, Console, XML, Webhook |
| **Middleware** | 6 | Split (by channel/deep-copy), Sync, Skip, Placeholder, Custom transform |
| **Output** | 7 | Screen, File, RTMP, RTSP, Image, Application, Fake (benchmark) |
| **Record** | 1 | Video & image recording, event-triggered capture |

> 📖 API reference đầy đủ: [**docs/NODES_AND_SAMPLES.md**](./docs/NODES_AND_SAMPLES.md)

---

## Samples Index

| Nhóm | Số lượng | Samples |
|------|----------|-------------|
| Pipeline Topology | 6 | `1-1-1`, `1-1-N`, `1-N-N`, `1-N-1-N`, `N-1-N`, `N-N` |
| PPE video (TensorRT / GPU, ONNX / CPU) | 1 | `ppe_video_sample` — hai nhánh người + PPE, ByteTrack, khung người/mũ/vest, trạng thái xanh/đỏ, MP4/CSV, analysis board và event MQTT lên HeraMind; [hướng dẫn](docs/PPE_VIDEO.md) |
| VLM / LLM (cần `CVEDIX_WITH_LLM=ON`) | 3 | `vlm_object_feature`, `rapidmedia_vlm_feature`, `event_snapshot_vlm_enrichment` |
| Benchmarks | 4 | `benchmark_llm`, `benchmark_pipeline`, `benchmark_face_recognizer`, `benchmark_people_analytics` (trong `benchmarks/`) |

> 📖 Chi tiết từng sample: [**docs/NODES_AND_SAMPLES.md**](./docs/NODES_AND_SAMPLES.md)

---

## Yêu cầu hệ thống

### Base

| Yêu cầu | Version |
|----------|---------|
| C++ Standard | C++20 |
| Compiler | GCC ≥ 7.5 |
| OpenCV | ≥ 4.6 |
| GStreamer | 1.14.5+ (required by OpenCV) |

### Optional (theo inference backend)

| Backend | Dependency | Ghi chú |
|---------|-----------|---------|
| TensorRT | CUDA + TensorRT | Auto-detect tại `/usr/local/tensorRT/` hoặc system include |
| PaddleInference | Paddle Inference | Tải bằng `scripts/download_paddle_inference.sh` |
| ONNX Runtime | ONNX Runtime | Auto-detect tại `third_party/onnxruntime/` |
| RKNN | RKNN Toolkit | Chỉ ARM64 RK3588 |
| mLLM | Ollama / vLLM / OpenAI API | Cần OpenSSL |

### Platform đã test

| Platform | Architecture | Hardware |
|----------|-------------|----------|
| Ubuntu 18.04+ | x86_64 | NVIDIA RTX/Tesla GPUs |
| Ubuntu 18.04+ | aarch64 | NVIDIA Jetson (TX2+) |
| Ubuntu 22.04+ | x86_64 | CPU-only (VMware/bare metal) |
| Ubuntu 18.04+ | aarch64 | Rockchip RK3588 NPU |
| Ubuntu 22.04+ | aarch64 | Ascend 310/910 |

---

## Build & Package Commands

| Lệnh | Mô tả |
|-------|-------|
| `make setup` | Cài dependencies (auto-detect hardware) |
| `make setup-auto` | Cài base dependencies (non-interactive) |
| `make build` | Build auto-detect hardware |
| `make build-cpu` | Build cho CPU only |
| `make build-nvidia-openvino-ort` | Build NVIDIA CUDA/TensorRT + OpenVINO + ONNX Runtime |
| `make build-rockchip` | Build cho Rockchip RK35xx |
| `make package-cpu` | Tạo `.deb` package cho CPU |
| `make package-rockchip` | Tạo `.deb` package cho Rockchip |
| `make info` | Hiện thông tin hardware |
| `make clean` | Xóa build directories |

### Install `.deb` package

```bash
sudo dpkg -i libcvedix-dev_*.deb
pkg-config --modversion cvedix
```

### CMake SDK integration

```cmake
find_package(cvedix REQUIRED)
target_link_libraries(my_app PRIVATE cvedix::cvedix_instance_sdk)
```

---

## Phát triển Node mới

### Quy trình tối thiểu

1. **Chọn loại node**:
   - Source → kế thừa `cvedix_src_node`
   - Middle → kế thừa `cvedix_node`
   - Destination → kế thừa `cvedix_des_node`

2. **Đặt file** theo nhóm chức năng trong `nodes/`

3. **Implement** các hàm chính:
   - `node_type()` — định danh loại node
   - `handle_frame_meta(...)` — xử lý frame
   - `handle_control_meta(...)` — xử lý control event (nếu cần)

4. **Thêm sample** trong `samples/` để verify

5. **Bọc optional deps** bằng macro:
   ```cpp
   #ifdef CVEDIX_WITH_TRT
   // TensorRT-specific code
   #endif
   ```
   Macros: `CVEDIX_WITH_TRT`, `CVEDIX_WITH_OPENVINO`, `CVEDIX_WITH_ORT`, `CVEDIX_WITH_RKNN`, `CVEDIX_WITH_PADDLE`, `CVEDIX_WITH_LLM`

---

## Minimal Example — Face Detection Pipeline

```cpp
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/infers/cvedix_face_detector_node.h>
#include <cvedix/utils/analysis_board/cvedix_analysis_board.h>
#include <cvedix/utils/logger/cvedix_logger.h>

#include <memory>

int main(int argc, char** argv) {
    CVEDIX_LOGGER_INIT();

    // Đường dẫn truyền qua argv để không hardcode trong source
    const std::string video_path = argc > 1 ? argv[1] : "video.mp4";
    const std::string model_path = argc > 2 ? argv[2] : "face_detection_yunet_2023mar.onnx";

    // 1. Create nodes
    auto source   = std::make_shared<cvedix_nodes::cvedix_file_src_node>("src", 0, video_path, 1.0);
    auto detector = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("det", model_path);

    // 2. Build pipeline
    detector->attach_to({source});

    // 3. Run
    source->start();

    // 4. Debug visualizer (FPS, latency, queue stats)
    cvedix_utils::cvedix_analysis_board board({source});
    board.display();

    // 5. Teardown: tách toàn bộ pipeline trước khi thoát
    source->detach_recursively();

    return 0;
}
```

---

## Troubleshooting nhanh

| Vấn đề | Kiểm tra |
|---------|----------|
| TensorRT not found | `ls /usr/include/*/NvInfer.h` hoặc `/usr/local/tensorRT/include/NvInfer.h` |
| OpenVINO not found | `source /opt/intel/openvino/setupvars.sh` rồi build lại |
| ONNX Runtime disabled | Đảm bảo `third_party/onnxruntime/lib/libonnxruntime.so` tồn tại |
| Runtime missing `.so` | `ldd ./build/bin/<sample>` → thêm path vào `LD_LIBRARY_PATH` |
| Link errors sau khi đổi backend | `make clean && make build-<target>` |

> 📖 Debug chi tiết: xem [**docs/DEVELOPMENT.md**](./docs/DEVELOPMENT.md) phần 8

---

## Tài liệu nội bộ

> 📚 Mục lục đầy đủ: [**docs/README.md**](./docs/README.md)
> 🌐 Tài liệu người dùng SDK (public): [**sdk.cvedix.com**](https://sdk.cvedix.com) — source tại submodule [`sdk-docs/`](./sdk-docs/)

| Tài liệu | Nội dung |
|-----------|----------|
| [**Onboarding**](./docs/ONBOARDING.md) | Dev mới: setup → build → chạy sample đầu tiên |
| [**Architecture Guide**](./docs/ARCHITECTURE.md) | Pipeline architecture, node internals, data model, class hierarchy |
| [**Development Guide**](./docs/DEVELOPMENT.md) | Setup, build, CMake options, node workflow, troubleshooting |
| [**Nodes & Samples Reference**](./docs/NODES_AND_SAMPLES.md) | API reference cho 80+ nodes và samples |
| [**BA Crossline Usage**](./docs/guides/BA_CROSSLINE_USAGE.md) | Cấu hình crossline counting |
| [**BA Event Format**](./docs/guides/BA_NODE_EVENT_FORMAT.md) | Định dạng event JSON/XML cho behavior analysis |
| [**BA Event Extraction**](./docs/guides/BA_EVENT_EXTRACTION_INTEGRATION.md) | Tích hợp BA event extraction |
| [**PPE → HeraMind**](./integrations/heramind_ppe/README.md) | Đẩy event PPE (crop, bbox, tracking_id, event_id) lên HeraMind qua MQTT |
| [**VLM Object Feature**](./docs/guides/VLM_OBJECT_FEATURE_NODE.md) | Trích xuất đặc trưng object bằng VLM |
| [**Face Recognition (SeetaFace6)**](./docs/guides/FACE_RECOGNIZER_SEETAFACE6.md) | Setup SeetaFace6 face recognizer |
| [**Face Recognition Benchmark**](./docs/reports/FACE_RECOGNITION_BENCHMARK_REPORT.txt) | Kết quả benchmark face recognition |

---

## Third-party Dependencies

| Project | Vai trò |
|---------|---------|
| [OpenCV](https://github.com/opencv/opencv) | Computer vision core |
| [GStreamer](https://gstreamer.freedesktop.org/) | Multimedia framework (video decode/encode) |
| [ONNX Runtime](https://github.com/microsoft/onnxruntime) | Cross-platform inference |
| [PaddleOCR](https://github.com/PaddlePaddle/PaddleOCR) | OCR toolkit |
| [ByteTrack](https://github.com/ifzhang/ByteTrack) | Multi-object tracking |
| [YOLOv11](https://github.com/ultralytics/ultralytics) | Object detection models |

---

> **Core Runtime** — Proprietary software by CVEDIX. Liên hệ team lead nếu cần access hoặc hỗ trợ.
