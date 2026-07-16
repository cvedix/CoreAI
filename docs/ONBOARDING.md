# Onboarding — Dev mới bắt đầu tại đây

> Mục tiêu: từ máy trắng đến chạy được sample đầu tiên.
> Yêu cầu: Ubuntu 20.04/22.04, quyền `sudo`, ~10 GB dung lượng trống.

## 1. Đọc trước khi code (15 phút)

1. [README.md](../README.md) — tổng quan SDK, tính năng, node catalog
2. [ARCHITECTURE.md](ARCHITECTURE.md) — hiểu khái niệm **Node**, pipeline SRC → MID → DES
3. Lướt qua [NODES_AND_SAMPLES.md](NODES_AND_SAMPLES.md) — biết node nào đã có sẵn

## 2. Cài dependencies

```bash
# Non-interactive — cài base deps (OpenCV, GStreamer, build tools)
make setup-auto

# Hoặc interactive — chọn thêm deps theo hardware (CUDA, TensorRT, RKNN...)
make setup
```

Script thực tế nằm ở [../scripts/setup_dependencies.sh](../scripts/setup_dependencies.sh).

## 3. Build

```bash
# Auto-detect hardware (khuyến nghị lần đầu)
make build

# Hoặc chọn target cụ thể
make build-cpu                      # CPU only — nhanh nhất để bắt đầu
make build-nvidia-openvino-ort      # NVIDIA + OpenVINO + ONNX Runtime
make build-rockchip                 # Rockchip RK35xx (RKNN + RGA)
```

Kết quả build nằm trong `build/bin/`. Chi tiết CMake options: [DEVELOPMENT.md](DEVELOPMENT.md).

## 4. Chạy sample đầu tiên

```bash
# Sample topology cơ bản nhất: 1 source → 1 inference → 1 output
./build/bin/1-1-1_sample
```

Sample cần model file (`.onnx`) và video input — xem tham số trong [../samples/1-1-1_sample.cpp](../samples/1-1-1_sample.cpp). Model không được commit vào repo (bị gitignore) — hỏi team lead vị trí lưu model nội bộ.

Danh sách 9 samples hiện có: xem [NODES_AND_SAMPLES.md — Phần II](NODES_AND_SAMPLES.md#phần-ii-hướng-dẫn-samples).

## 5. Hiểu codebase

| Thư mục | Vai trò |
|---------|---------|
| `nodes/` | Toàn bộ node: src, des, infers, track, ba, broker, osd, mid, ffio, proc, record |
| `objects/` | Data model: `frame_meta`, `target`, control meta, shapes |
| `utils/` | Logger, analysis board, MQTT client, helpers |
| `samples/` | Chương trình mẫu minh họa pipeline |
| `third_party/` | Backend dependencies (ONNX Runtime, TensorRT, llama.cpp...) |
| `scripts/` | Build/setup/package scripts |

Quy tắc quan trọng:

- Pipeline dựng bằng `node->attach_to({upstream})`, chạy bằng `src->start()`
- Mỗi source/destination node bắt buộc có `channel_index` riêng
- Code phụ thuộc backend phải bọc macro: `#ifdef CVEDIX_WITH_TRT`, `CVEDIX_WITH_RKNN`, `CVEDIX_WITH_LLM`...
- Debug pipeline bằng `cvedix_analysis_board` (hiển thị FPS, latency, queue realtime)

## 6. Viết node đầu tiên

Xem mục "Phát triển Node mới" trong [README.md](../README.md) và tham khảo node đơn giản nhất: [../nodes/mid/cvedix_skip_node.h](../nodes/mid/cvedix_skip_node.h).

Quy trình tóm tắt:

1. Kế thừa `cvedix_src_node` / `cvedix_node` / `cvedix_des_node`
2. Implement `node_type()` và `handle_frame_meta(...)`
3. Đặt file đúng nhóm chức năng trong `nodes/`
4. Thêm sample vào `samples/` + đăng ký trong `samples/CMakeLists.txt` để verify

## 7. Gặp lỗi?

- Xem bảng Troubleshooting trong [README.md](../README.md) và [DEVELOPMENT.md](DEVELOPMENT.md) phần 8
- Build lỗi sau khi đổi backend: `make clean && make build-<target>`
- Vẫn kẹt → hỏi team lead

## 8. Checklist hoàn thành onboarding

- [ ] Build thành công `make build-cpu`
- [ ] Chạy được `1-1-1_sample` với video + model thật
- [ ] Đọc xong ARCHITECTURE.md, hiểu SRC/MID/DES và `attach_to()`
- [ ] Biết vị trí tài liệu: [docs/README.md](README.md)
