# YOLOv11 ONNX Detector Node

## Tổng quan

`cvedix_yolov11_detector_node` là node detector sử dụng mô hình YOLOv11 ONNX với OpenCV DNN backend. Node này hỗ trợ:
- YOLOv11 anchor-free detection
- ONNX model format
- Chạy trên bất kỳ platform nào (CPU/GPU với OpenCV DNN)
- Không phụ thuộc vào Rockchip NPU

## So sánh với RKNN YOLOv11

| Feature | YOLOv11 ONNX | YOLOv11 RKNN |
|---------|--------------|--------------|
| **Backend** | OpenCV DNN | RKNN Runtime |
| **Model Format** | .onnx | .rknn |
| **Hardware** | CPU/GPU (bất kỳ) | Rockchip NPU only |
| **Speed** | Tùy platform | Rất nhanh trên Rockchip |
| **Flexibility** | Portable | Rockchip only |

## Build

```bash
mkdir build && cd build

# Minimal (chỉ cần GStreamer)
cmake -DCVEDIX_WITH_GSTREAMER=ON -DCVEDIX_BUILD_SAMPLES=ON ..

# Full build
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_BUILD_SAMPLES=ON \
    ..

make -j$(nproc)
```

## Export ONNX Model

### Từ YOLOv11 Ultralytics

```python
from ultralytics import YOLO

# Load model
model = YOLO('yolov11n.pt')

# Export to ONNX
model.export(format='onnx', 
             imgsz=640,
             simplify=True,
             opset=12)

# Output: yolov11n.onnx
```

### Verify ONNX Model

```bash
# Kiểm tra input/output shapes
python3 -c "
import onnx
model = onnx.load('yolov11n.onnx')
print('Input:', [input.name for input in model.graph.input])
print('Output:', [output.name for output in model.graph.output])
"
```

## Sử dụng

### 1. Chuẩn bị Model và Labels

```bash
# Model
cp yolov11n.onnx ./cvedix_data/models/

# Labels file (COCO 80 classes)
cat > ./cvedix_data/models/coco_80_labels_list.txt << 'EOF'
person
bicycle
car
motorcycle
airplane
bus
train
truck
boat
traffic light
# ... (80 classes total)
EOF
```

### 2. Chạy Sample

```bash
cd build/bin

# Với cấu hình mặc định
./yolov11_onnx_detector_sample

# Với model và video tùy chỉnh
./yolov11_onnx_detector_sample \
    ./path/to/yolov11n.onnx \
    ./path/to/video.mp4 \
    ./path/to/labels.txt
```

### 3. Sử dụng trong Code

```cpp
#include "cvedix/nodes/infers/cvedix_yolov11_detector_node.h"

// Tạo detector
auto detector = std::make_shared<cvedix_nodes::cvedix_yolov11_detector_node>(
    "yolov11_detector",
    "./models/yolov11n.onnx",
    "./models/coco_labels.txt",
    640,   // Input width
    640,   // Input height
    80,    // Number of classes
    0.25f, // Score threshold
    0.45f  // NMS threshold
);

// Attach to pipeline
detector->attach_to({source_node});
```

## Model Variants

YOLOv11 có nhiều biến thể với trade-off giữa speed và accuracy:

| Model | Size | mAPval | Speed (CPU) | Speed (GPU) |
|-------|------|---------|-------------|-------------|
| YOLOv11n | 5MB | 39.5 | ~30 FPS | ~100 FPS |
| YOLOv11s | 18MB | 47.0 | ~15 FPS | ~80 FPS |
| YOLOv11m | 40MB | 51.5 | ~8 FPS | ~50 FPS |
| YOLOv11l | 50MB | 53.4 | ~5 FPS | ~40 FPS |
| YOLOv11x | 110MB | 54.7 | ~3 FPS | ~30 FPS |

*Speeds are approximate and depend on hardware*

## Configuration

### Điều chỉnh Thresholds

```cpp
auto detector = std::make_shared<cvedix_nodes::cvedix_yolov11_detector_node>(
    "detector", model_path, labels_path,
    640, 640, 80,
    0.5f,  // score_threshold: Higher = less detections, more precision
    0.3f   // nms_threshold: Lower = less overlapping boxes
);
```

### Custom Input Size

```cpp
// Smaller size = faster, less accuracy
auto detector = std::make_shared<cvedix_nodes::cvedix_yolov11_detector_node>(
    "detector", model_path, labels_path,
    320, 320,  // Half size → ~4x faster
    80, 0.25f, 0.45f
);
```

**Lưu ý**: Model cần được export với đúng input size.

## Troubleshooting

### Lỗi: "Failed to load ONNX model"

```bash
# Kiểm tra OpenCV có DNN support
python3 -c "import cv2; print('DNN:', cv2.dnn.DNN_BACKEND_OPENCV)"

# Kiểm tra model file tồn tại
ls -lh model.onnx
```

### Lỗi: "Incorrect output dimensions"

Model ONNX cần đúng format YOLOv11:
- Input: [1, 3, 640, 640] (NCHW format)
- Output: [1, 84, 8400] cho 80 classes

Verify model:
```bash
python3 -c "
import onnx
model = onnx.load('yolov11n.onnx')
for output in model.graph.output:
    print(output.name, output.type)
"
```

### Performance thấp

1. Sử dụng GPU backend nếu có:
```bash
# OpenCV build với CUDA support
# Hoặc dùng RKNN version cho Rockchip
```

2. Giảm input size (320x320 thay vì 640x640)

3. Sử dụng model nhỏ hơn (yolov11n thay vì yolov11x)

## Tài liệu tham khảo

- [YOLOv11 Ultralytics](https://docs.ultralytics.com/models/yolov11/)
- [OpenCV DNN Documentation](https://docs.opencv.org/master/d2/d58/tutorial_table_of_content_dnn.html)
- [ONNX Documentation](https://onnx.ai/onnx/intro/)

## Example Output

```
[Info] ==================================================
[Info] YOLOv11 ONNX Detector Sample (OpenCV DNN)
[Info] ==================================================
[Info] Model: ./yolov11n.onnx
[Info] Video: ./test_video.mp4
[Info] Labels: ./coco_labels.txt
[Info] ==================================================
[Info] [yolov11_detector] YOLOv11 ONNX detector initialized: 640x640, 80 classes, score_thresh=0.25, nms_thresh=0.45
[Info] Pipeline built. Starting processing...
[Debug] [yolov11_detector] Detected 15 objects before NMS, 8 after NMS
[Debug] [yolov11_detector] Added 8 targets to frame_meta
```

---

**Last Updated**: 2025-12-04  
**Version**: 2025.0.1.2

