# RKNN YOLOv8 Face Detection Sample

## Tổng quan

Sample đơn giản sử dụng YOLOv8 model đã được train cho face detection và convert sang RKNN format để chạy trên Rockchip NPU.

## Model: yolov8n_face_detection.rknn

- **Architecture**: YOLOv8 nano
- **Task**: Face detection (1 class)
- **Input**: 640x640 RGB
- **Platform**: Rockchip NPU (RK3566/RK3568/RK3588)
- **Format**: RKNN (converted from ONNX/PyTorch)

## Build

```bash
mkdir build && cd build

cmake \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_WITH_RGA=ON \
    -DCVEDIX_BUILD_SAMPLES=ON \
    ..

make -j$(nproc)
```

## Chuẩn bị Model

### Option 1: Sử dụng model có sẵn

Nếu đã có model `yolov8n_face_detection.rknn`, copy vào:

```bash
mkdir -p ./cvedix_data/models/face/
cp yolov8n_face_detection.rknn ./cvedix_data/models/face/
```

### Option 2: Convert từ YOLOv8 pretrained

**Bước 1: Train YOLOv8 cho face detection (hoặc download pretrained)**

```python
# train_yolov8_face.py
from ultralytics import YOLO

# Load YOLOv8 nano
model = YOLO('yolov8n.pt')

# Train on face dataset (hoặc load pretrained face model)
# model.train(data='face_dataset.yaml', epochs=100)

# Export to ONNX
model.export(format='onnx', imgsz=640, simplify=True)
# → yolov8n.onnx
```

**Bước 2: Convert ONNX to RKNN**

```python
# convert_to_rknn.py
from rknn.api import RKNN

rknn = RKNN()

# Config for RK3588 (hoặc rk3566, rk3568)
rknn.config(target_platform='rk3588')

# Load ONNX model
print('Loading ONNX model...')
ret = rknn.load_onnx('yolov8n.onnx')
if ret != 0:
    print('Load model failed!')
    exit(ret)

# Build with quantization
print('Building RKNN model...')
ret = rknn.build(do_quantization=True, dataset='./dataset.txt')
if ret != 0:
    print('Build failed!')
    exit(ret)

# Export RKNN
print('Exporting RKNN model...')
ret = rknn.export_rknn('yolov8n_face_detection.rknn')
if ret != 0:
    print('Export failed!')
    exit(ret)

print('Done!')
rknn.release()
```

**dataset.txt** (cho quantization):
```
./images/face1.jpg
./images/face2.jpg
./images/face3.jpg
# ... (ít nhất 100 ảnh)
```

## Chạy Sample

### Với cấu hình mặc định:

```bash
cd build/bin

# Chạy với video và model mặc định
./rknn_yolov8_face_detection_simple_sample
```

### Với model và video tùy chỉnh:

```bash
./rknn_yolov8_face_detection_simple_sample \
    ./cvedix_data/models/face/yolov8n_face_detection.rknn \
    ./cvedix_data/test_video/face.mp4
```

## Output mong đợi

```
[Info] ==================================================
[Info] RKNN YOLOv8 Face Detection Sample
[Info] ==================================================
[Info] Model: ./cvedix_data/models/face/yolov8n_face_detection.rknn
[Info] Video: ./cvedix_data/test_video/face.mp4
[Info] ==================================================
[Info] [yolov8_face_detector] RKNN helper initialized
[Info] [yolov8_face_detector] Model loaded successfully
[Info] Pipeline started. Press Ctrl+C to stop...
[Debug] [yolov8_face_detector] Detected 3 faces
```

Bạn sẽ thấy:
- Video hiển thị trên màn hình
- Bounding boxes màu xanh quanh khuôn mặt
- Confidence scores hiển thị trên mỗi box

## Performance

### RK3588 (6 TOPS NPU)
- **YOLOv8n**: ~50-60 FPS
- **YOLOv8s**: ~30-40 FPS
- Input 640x640

### RK3566/RK3568 (1 TOPS NPU)
- **YOLOv8n**: ~15-20 FPS
- Input 640x640

*Performance với RGA enabled (~10-15% faster)*

## Troubleshooting

### Lỗi: "Failed to load RKNN model"

```bash
# Kiểm tra model file tồn tại
ls -lh ./cvedix_data/models/face/yolov8n_face_detection.rknn

# Kiểm tra RKNN runtime
ldconfig -p | grep rknnrt
```

### Lỗi: "RKNN runtime version mismatch"

Model được convert với toolkit version phải khớp với runtime version:

```bash
# Update RKNN runtime
cd ~/rknn-toolkit2/rknpu2/runtime/Linux/librknn_api/aarch64
sudo cp librknnrt.so /usr/local/lib/
sudo ldconfig

# Convert lại model với toolkit mới
```

### Không detect được face

1. Kiểm tra threshold (có thể quá cao):
```cpp
// Giảm threshold
auto face_detector = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
    "detector", model_path,
    0.3f,  // Giảm từ 0.5 xuống 0.3
    0.5f,
    640, 640, 1
);
```

2. Kiểm tra ánh sáng và góc camera
3. Kiểm tra model có đúng là face detection model không

## So sánh Nodes

| Node | Model Type | Use Case |
|------|------------|----------|
| `cvedix_rknn_yolov8_detector_node` | YOLOv8 (general object) | Face detection với YOLOv8 |
| `cvedix_rknn_face_detector_node` | YuNet (face-specific) | Face detection với YuNet |
| `cvedix_yunet_face_detector_node` | YuNet ONNX | Face detection CPU/GPU |

Sample này dùng `cvedix_rknn_yolov8_detector_node` với model YOLOv8 đã train cho face detection.

## Tài liệu tham khảo

- [YOLOv8 Ultralytics](https://docs.ultralytics.com/)
- [RKNN Toolkit2](https://github.com/airockchip/rknn-toolkit2)
- [Face Detection Dataset](https://github.com/ultralytics/yolov5/wiki/Train-Custom-Data)

---

**Last Updated**: 2025-12-07  
**Model**: yolov8n_face_detection.rknn

