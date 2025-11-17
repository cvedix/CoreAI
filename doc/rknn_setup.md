# RKNN Setup Guide

Hướng dẫn cài đặt và sử dụng RKNN (Rockchip NPU) và RGA (Rockchip Graphics Accelerator) cho InstancePipeline.

## Tổng quan

Module `cvedix_yolo_rknn_face_detector` sử dụng Rockchip NPU để tăng tốc inference cho face detection. Module này hỗ trợ:
- **RK3566**: Entry-level NPU
- **RK3568**: Mid-range NPU  
- **RK3588**: High-end NPU (Rock 5, Orange Pi 5, Radxa Zero 3)

RGA được sử dụng để tăng tốc preprocessing (resize, color conversion) bằng phần cứng.

## Yêu cầu

- Ubuntu 18.04/20.04/22.04 (aarch64)
- Rockchip SoC với NPU (RK3566/68/88)
- CMake >= 3.10
- C++17 compiler

## Cài đặt RKNN Toolkit

### 1. Tải RKNN Toolkit2

```bash
cd ~
git clone https://github.com/airockchip/rknn-toolkit2.git
cd rknn-toolkit2
```

### 2. Cài đặt Runtime Library

Chỉ cần runtime library cho inference (không cần full toolkit):

```bash
cd rknn-toolkit2/rknpu2/runtime/Linux/librknn_api/aarch64
sudo cp librknnrt.so /usr/local/lib/
sudo ldconfig

cd ../include
sudo cp rknn_api.h /usr/local/include/
sudo cp rknn_custom_op.h /usr/local/include/  # optional
sudo cp rknn_matmul_api.h /usr/local/include/  # optional
```

### 3. Xác minh cài đặt

```bash
# Kiểm tra library
ldconfig -p | grep rknnrt

# Kiểm tra headers
ls /usr/local/include/rknn_api.h
```

## Cài đặt RGA Library

RGA (Rockchip Graphics Accelerator) cung cấp hardware acceleration cho image processing.

### 1. Cài đặt từ package manager (nếu có)

```bash
sudo apt-get update
sudo apt-get install librga-dev
```

### 2. Hoặc build từ source

```bash
# Clone RGA repository (nếu có)
git clone https://github.com/rockchip-linux/linux-rga.git
cd linux-rga
# Follow build instructions in repository
```

### 3. Xác minh cài đặt

```bash
# Kiểm tra library
ldconfig -p | grep rga

# Kiểm tra headers
ls /usr/local/include/rga.h
```

**Lưu ý**: RGA là optional. Nếu không có RGA, code sẽ tự động fallback về OpenCV.

## Chuyển đổi Model

### 1. Cài đặt RKNN Toolkit2 (Python)

RKNN Toolkit2 cần để convert model từ ONNX/PyTorch sang RKNN format.

```bash
# Cài đặt Python dependencies
pip3 install numpy opencv-python onnx onnxruntime

# Cài đặt RKNN Toolkit2
cd ~/rknn-toolkit2
pip3 install -r requirements.txt
pip3 install packages/rknn_toolkit2-*.whl
```

### 2. Convert YoloV8 Face Detection Model

Tạo script convert model:

```python
# convert_yolov8_face.py
from rknn.api import RKNN

def convert_onnx_to_rknn(onnx_path, rknn_path, target_platform='rk3588'):
    rknn = RKNN(verbose=True)
    
    # Pre-process config
    print('--> Config model')
    rknn.config(mean_values=[[0, 0, 0]], std_values=[[255, 255, 255]], 
                target_platform=target_platform)
    print('done')
    
    # Load ONNX model
    print('--> Loading model')
    ret = rknn.load_onnx(model=onnx_path)
    if ret != 0:
        print('Load model failed!')
        return False
    print('done')
    
    # Build model
    print('--> Building model')
    ret = rknn.build(do_quantization=True, dataset='./dataset.txt')
    if ret != 0:
        print('Build model failed!')
        return False
    print('done')
    
    # Export RKNN model
    print('--> Export rknn model')
    ret = rknn.export_rknn(rknn_path)
    if ret != 0:
        print('Export rknn model failed!')
        return False
    print('done')
    
    rknn.release()
    return True

if __name__ == '__main__':
    # Convert model
    onnx_path = 'yolov8n_face.onnx'
    rknn_path = 'yolov8n_face_detection.rknn'
    target = 'rk3588'  # or 'rk3566', 'rk3568'
    
    convert_onnx_to_rknn(onnx_path, rknn_path, target)
```

### 3. Tạo Dataset cho Quantization

Tạo file `dataset.txt` chứa danh sách ảnh để calibration:

```
./images/img1.jpg
./images/img2.jpg
./images/img3.jpg
...
```

## Build InstancePipeline với RKNN Support

### 1. Configure CMake

```bash
cd /path/to/instance_pipeline
mkdir build && cd build

# Build với RKNN và RGA support
cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_WITH_RGA=ON ..

# Hoặc chỉ RKNN (không có RGA)
cmake -DCVEDIX_WITH_RKNN=ON ..
```

### 2. Build

```bash
make -j8
```

### 3. Kiểm tra build

```bash
# Kiểm tra sample đã được build
ls build/bin/rknn_face_detector_sample
```

## Sử dụng

### 1. Chuẩn bị Model

Đặt file `.rknn` model vào thư mục models:

```bash
mkdir -p ./cvedix_data/models/face
cp yolov8n_face_detection.rknn ./cvedix_data/models/face/
```

### 2. Chạy Sample

```bash
cd build/bin
./rknn_face_detector_sample
```

### 3. Sử dụng trong Code

```cpp
#include "../nodes/infers/cvedix_yolo_rknn_face_detector_node.h"

// Tạo RKNN face detector
auto rknn_detector = std::make_shared<cvedix_nodes::cvedix_yolo_rknn_face_detector_node>(
    "rknn_face_detector",
    "./models/face/yolov8n_face_detection.rknn",  // model path
    0.5,  // score threshold
    0.5,  // NMS threshold
    640,  // input width
    640   // input height
);
```

## RGA Optimization

RGA (Rockchip Graphics Accelerator) được tích hợp để tăng tốc preprocessing operations:

### Implemented Operations

1. **Hardware-Accelerated Resize**: Sử dụng `imresize()` của RGA thay vì OpenCV
   - Tăng tốc đáng kể cho resize operations
   - Hỗ trợ các format: GRAY, RGB_888, RGBA_8888

2. **Hardware-Accelerated Color Conversion**: Sử dụng `imcvtcolor()` cho BGR↔RGB swap
   - Rất hiệu quả cho ML pipelines thường cần convert BGR→RGB
   - Sử dụng `IM_COLOR_SPACE_SWAP_RB` để swap R và B channels

3. **Crop-Resize**: Kết hợp crop và resize trong hardware
   - Sử dụng `imcrop()` và `imresize()` của RGA
   - Hiệu quả hơn so với crop + resize riêng biệt

4. **Automatic Fallback**: Tự động fallback về OpenCV nếu:
   - RGA không available
   - Operation không được RGA hỗ trợ
   - RGA operation thất bại

### RGA API Integration

Implementation sử dụng librga API:
- `wrapbuffer_virtualaddr()`: Wrap OpenCV Mat data vào RGA buffer
- `imresize()`: Hardware-accelerated resize
- `imcvtcolor()`: Hardware-accelerated color conversion
- `imcrop()`: Hardware-accelerated crop
- `releasebuffer_handle()`: Release RGA buffer handles

### Performance Benefits

- **Resize**: 3-5x faster than OpenCV on Rockchip hardware
- **Color Conversion**: 2-3x faster for BGR↔RGB swap
- **CPU Usage**: Giảm CPU load đáng kể, giải phóng CPU cho inference

## Performance Tips

1. **Sử dụng RGA**: Enable RGA để tăng tốc preprocessing (resize, color conversion)
   - RGA có thể tăng tốc resize lên 3-5x so với OpenCV
   - Đặc biệt hiệu quả cho real-time video processing

2. **Input Size**: Sử dụng input size nhỏ hơn (320x320) để tăng FPS nếu độ chính xác cho phép
   - RGA resize rất nhanh, có thể dùng input size lớn hơn mà vẫn đạt FPS cao

3. **Batch Processing**: RKNN hỗ trợ batch inference, có thể optimize thêm
4. **Model Quantization**: Sử dụng int8 quantized model để tăng tốc

## Troubleshooting

### Lỗi: "RKNN library not found"
- Kiểm tra `librknnrt.so` đã được copy vào `/usr/local/lib`
- Chạy `sudo ldconfig` để update library cache

### Lỗi: "RKNN headers not found"
- Kiểm tra `rknn_api.h` đã được copy vào `/usr/local/include`
- Kiểm tra CMake đã tìm thấy headers

### Lỗi: "RGA not available"
- RGA là optional, code sẽ fallback về OpenCV
- Để enable RGA, cài đặt `librga.so` và headers

### Model không chạy
- Kiểm tra model đã được convert đúng cho target platform (rk3566/68/88)
- Kiểm tra input size khớp với model requirements
- Xem logs để debug

## Tài liệu tham khảo

- [RKNN Toolkit2 GitHub](https://github.com/airockchip/rknn-toolkit2)
- [Rockchip NPU Documentation](https://www.rock-chips.com/)
- [YoloV8-NPU Example](https://github.com/Qengineering/YoloV8-NPU)

## Performance Benchmarks

Dựa trên [YoloV8-NPU benchmarks](https://github.com/Qengineering/YoloV8-NPU):

| Model | Platform | FPS | Notes |
|-------|----------|-----|-------|
| YoloV8n | RK3588 | ~53 | int8 quantized |
| YoloV8n | RK3566/68 | ~18 | int8 quantized |

*Performance có thể khác nhau tùy vào input size và model configuration.*

