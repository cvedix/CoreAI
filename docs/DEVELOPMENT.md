# Development Guide

Tài liệu này dành cho developer làm việc trực tiếp trên source code Core Runtime. Nếu chỉ muốn dùng SDK đã đóng gói, xem phần `.deb` package trong README.

## 1. Chuẩn bị môi trường

### Hệ điều hành khuyến nghị

| Môi trường | Ghi chú |
|------------|---------|
| Ubuntu 20.04/22.04 x86_64 | Phù hợp cho CPU, NVIDIA GPU, OpenVINO, ONNX Runtime |
| Ubuntu/JetPack aarch64 | Phù hợp cho NVIDIA Jetson |
| Ubuntu aarch64 trên RK3588 | Phù hợp cho Rockchip RKNN/RGA |

### Dependency cơ bản

Từ thư mục gốc repo:

```bash
make setup-auto
```

Lệnh này cài các dependency nền tảng như compiler, CMake, OpenCV, GStreamer, Eigen, OpenSSL và RTSP server headers.

Nếu cần chọn thêm dependency theo phần cứng:

```bash
make setup
```

Các dependency phần cứng như CUDA, TensorRT, OpenVINO, Paddle Inference, RKNN thường cần cài thủ công theo hướng dẫn vendor.

## 2. Build nhanh

### Build mặc định

```bash
make build
```

Target này chạy:

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Build CPU kèm samples

```bash
make build-cpu
```

Script tương ứng:

```bash
scripts/build_with_cpu.sh
```

### Build NVIDIA + OpenVINO + ONNX Runtime

```bash
make build-nvidia-openvino-ort
```

Target này bật:

```bash
-DCVEDIX_WITH_CUDA=ON
-DCVEDIX_WITH_TRT=ON
-DCVEDIX_WITH_OPENVINO=ON
-DCVEDIX_BUILD_SAMPLES=ON
```

ONNX Runtime được bật tự động khi tồn tại:

```bash
third_party/onnxruntime/lib/libonnxruntime.so
third_party/onnxruntime/include
```

Nếu ONNX Runtime nằm ở vị trí khác:

```bash
ONNXRUNTIME_DIR=/path/to/onnxruntime make build-nvidia-openvino-ort
```

Có thể đổi thư mục build hoặc tắt samples:

```bash
BUILD_DIR=build_nv_ov_ort BUILD_SAMPLES=OFF make build-nvidia-openvino-ort
```

Nếu không cần LLM:

```bash
WITH_LLM=OFF make build-nvidia-openvino-ort
```

### Build Rockchip

```bash
make build-rockchip
```

Target này bật RKNN và RGA:

```bash
-DCVEDIX_WITH_RKNN=ON
-DCVEDIX_WITH_RGA=ON
```

## 3. Build thủ công bằng CMake

Khi cần kiểm soát option chi tiết:

```bash
mkdir -p build
cd build
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCVEDIX_WITH_CUDA=ON \
  -DCVEDIX_WITH_TRT=ON \
  -DCVEDIX_WITH_OPENVINO=ON \
  -DCVEDIX_BUILD_SAMPLES=ON \
  ..
make -j$(nproc)
```

Các option thường dùng:

| Option | Mặc định | Mô tả |
|--------|----------|-------|
| `CVEDIX_WITH_CUDA` | `OFF` | Bật CUDA cho OpenCV DNN hoặc backend CUDA |
| `CVEDIX_WITH_TRT` | `OFF` | Bật TensorRT backend |
| `CVEDIX_WITH_OPENVINO` | `OFF` | Bật OpenVINO backend |
| `CVEDIX_WITH_PADDLE` | `OFF` | Bật Paddle Inference |
| `CVEDIX_WITH_LLM` | `OFF` | Bật mLLM node, cần OpenSSL |
| `CVEDIX_WITH_RKNN` | `OFF` | Bật Rockchip RKNN |
| `CVEDIX_WITH_RGA` | `OFF` | Bật Rockchip RGA |
| `CVEDIX_BUILD_SAMPLES` | `OFF` | Build sample programs |
| `ONNXRUNTIME_DIR` | `third_party/onnxruntime` | Root của ONNX Runtime |

## 4. Chạy sample

Khi `CVEDIX_BUILD_SAMPLES=ON`, binary sample được xuất ra:

```bash
build/bin
```

Ví dụ:

```bash
./build/bin/1-1-1_sample
./build/bin/1-1-N_sample
./build/bin/N-N_sample
```

Một số sample yêu cầu model, video input hoặc backend cụ thể. Xem thêm:

- [Nodes & Samples Reference](./NODES_AND_SAMPLES.md)
- [Onboarding](./ONBOARDING.md)

## 5. Cấu trúc source chính

| Thư mục | Vai trò |
|---------|---------|
| `nodes/` | Các node xử lý pipeline: source, inference, tracking, BA, OSD, destination |
| `objects/` | Metadata, frame object, control/event data |
| `utils/` | Logger, analysis board, helper, client, utility |
| `third_party/` | Backend/plugin/vendor dependencies |
| `samples/` | Chương trình mẫu để kiểm thử pipeline |
| `scripts/` | Script setup, build, package, bundle |
| `docs/` | Tài liệu kiến trúc, node, sample và tích hợp |

## 6. Quy trình phát triển khuyến nghị

1. Kiểm tra phần cứng và toolchain:

```bash
make info
cmake --version
g++ --version
```

2. Build sạch khi đổi backend lớn:

```bash
make clean
make build-cpu
```

3. Khi phát triển node mới, ưu tiên build samples:

```bash
BUILD_SAMPLES=ON make build-nvidia-openvino-ort
```

4. Chạy sample nhỏ trước khi chạy pipeline phức tạp:

```bash
./build/bin/1-1-N_sample
./build/bin/1-1-1_sample
```

5. Với lỗi link runtime, kiểm tra library path:

```bash
ldd ./build/bin/1-1-1_sample
ldconfig -p | grep -E "onnxruntime|openvino|nvinfer"
```

## 7. Thêm một node mới

Quy trình tối thiểu:

1. Chọn loại node phù hợp:
   - Source node: kế thừa từ `cvedix_src_node`
   - Middle node: kế thừa từ `cvedix_node`
   - Destination node: kế thừa từ `cvedix_des_node`

2. Đặt file theo nhóm chức năng trong `nodes/`.

3. Implement các hàm xử lý chính:
   - `node_type()`
   - `handle_frame_meta(...)`
   - `handle_control_meta(...)` nếu node cần xử lý control event

4. Thêm sample nhỏ trong `samples/` để chứng minh node hoạt động.

5. Nếu node phụ thuộc backend optional, bọc bằng macro tương ứng:
   - `CVEDIX_WITH_TRT`
   - `CVEDIX_WITH_OPENVINO`
   - `CVEDIX_WITH_ORT`
   - `CVEDIX_WITH_RKNN`
   - `CVEDIX_WITH_PADDLE`
   - `CVEDIX_WITH_LLM`

## 8. Debug lỗi build thường gặp

### TensorRT not found

Kiểm tra header:

```bash
ls /usr/include/x86_64-linux-gnu/NvInfer.h
ls /usr/include/aarch64-linux-gnu/NvInfer.h
ls /usr/local/tensorRT/include/NvInfer.h
```

Nếu không có, cài TensorRT theo NVIDIA package hoặc đặt TensorRT ở `/usr/local/tensorRT`.

### OpenVINO not found

Kiểm tra OpenVINO CMake package:

```bash
source /opt/intel/openvino/setupvars.sh
cmake --find-package -DNAME=OpenVINO -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST
```

### ONNX Runtime disabled

CMake chỉ bật ORT khi có:

```bash
${ONNXRUNTIME_DIR}/lib/libonnxruntime.so
${ONNXRUNTIME_DIR}/include
```

Chạy lại với đường dẫn đúng:

```bash
ONNXRUNTIME_DIR=/path/to/onnxruntime make build-nvidia-openvino-ort
```

### Runtime không tìm thấy `.so`

Kiểm tra:

```bash
ldd ./build/bin/<sample_name>
```

Nếu thiếu library, thêm vào `LD_LIBRARY_PATH`, ví dụ:

```bash
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:third_party/onnxruntime/lib:$LD_LIBRARY_PATH
```

## 9. Package

Build package CPU:

```bash
make package-cpu
```

Build package Rockchip:

```bash
make package-rockchip
```

Sau khi tạo `.deb`:

```bash
sudo dpkg -i libcvedix-dev_*.deb
pkg-config --modversion cvedix
```
