# TensorRT Samples

## Tổng quan

Các samples này demo sử dụng TensorRT để tối ưu hóa inference performance trên NVIDIA GPUs.

---

## Sample 1: TensorRT Inference

### File
`trt_infer_sample.cpp`

### Mô tả
Vehicle và plate detection sử dụng TensorRT optimized models.

### Pipeline
```
Video → Vehicle Detector (TRT) → Plate Detector (TRT) → OSD → [Screen, File, RTMP]
```

### Features
- TensorRT engine inference
- Vehicle detection
- License plate detection
- Multiple outputs

### Usage
```bash
./build/bin/trt_infer_sample
```

### Requirements
- Build với `-DCVEDIX_WITH_TRT=ON`
- CUDA và cuDNN
- TensorRT installed

### Configuration
```cpp
auto vehicle_detector = std::make_shared<cvedix_trt_vehicle_detector>(
    "vehicle_detector",
    "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt"
);

auto plate_detector = std::make_shared<cvedix_trt_vehicle_plate_detector>(
    "plate_detector",
    "./cvedix_data/models/trt/plate/det_v8.5.trt",
    "./cvedix_data/models/trt/plate/rec_v8.5.trt"
);
```

---

## Sample 2: TensorRT YOLOv8

### Files
- `trt_yolov8_sample.cpp`
- `trt_yolov8_sample2.cpp`

### Mô tả
Object detection sử dụng TensorRT optimized YOLOv8.

### Pipeline
```
Video → YOLOv8 TRT Detector → OSD → Screen
```

### Features
- YOLOv8 TensorRT engine
- Multi-class detection
- High performance

### Usage
```bash
./build/bin/trt_yolov8_sample
./build/bin/trt_yolov8_sample2
```

### Configuration
```cpp
auto detector = std::make_shared<cvedix_trt_yolov8_detector_node>(
    "detector",
    "./cvedix_data/models/trt/yolov8/yolov8n.trt",
    0.5,  // score threshold
    0.4   // NMS threshold
);
```

---

## Sample 3: Multi TensorRT Inference Nodes

### File
`multi_trt_infer_nodes_sample.cpp`

### Mô tả
Sử dụng nhiều TensorRT inference nodes song song.

### Pipeline
```
Video → [TRT Detector1, TRT Detector2, ...] → Merge → OSD → Screen
```

### Features
- Parallel TensorRT inference
- Multiple models
- Performance optimization

### Usage
```bash
./build/bin/multi_trt_infer_nodes_sample
```

### Use Case
- Run multiple models simultaneously
- Compare model performance
- Multi-task processing

---

## Sample 4: InsightFace TensorRT

### Files
- `insightface_trt_sample.cpp`
- `insightface_register_recognize_face_trt_sample.cpp`

### Mô tả
Face recognition sử dụng InsightFace với TensorRT optimization.

📖 Xem chi tiết: [README_INSIGHTFACE_TRT.md](README_INSIGHTFACE_TRT.md)

---

## So sánh các TensorRT Samples

| Sample | Model | Use Case | Performance |
|--------|-------|----------|-------------|
| **trt_infer** | Vehicle + Plate | Traffic monitoring | Excellent |
| **trt_yolov8** | YOLOv8 | General detection | Excellent |
| **multi_trt_infer** | Multiple models | Parallel processing | Excellent |
| **insightface_trt** | InsightFace | Face recognition | Excellent |

---

## Build

### Requirements
- CUDA (>= 11.0)
- cuDNN (>= 8.0)
- TensorRT (>= 8.0)

### Build Command
```bash
cd build
cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON ..
make trt_infer_sample
make trt_yolov8_sample
make multi_trt_infer_nodes_sample
```

---

## Model Conversion

### ONNX to TensorRT
```bash
# Using trtexec
trtexec \
    --onnx=model.onnx \
    --saveEngine=model.trt \
    --fp16 \
    --workspace=4096
```

### Python Conversion
```python
import tensorrt as trt

# Load ONNX
onnx_path = "model.onnx"
engine_path = "model.trt"

# Convert to TensorRT
# (Use TensorRT Python API)
```

---

## Performance

### GPU Performance (RTX 3090)
- **Vehicle Detection**: ~5-8ms per frame
- **YOLOv8 Detection**: ~3-6ms per frame
- **Face Recognition**: ~2-4ms per frame

### Optimization Tips
1. Use FP16 precision
2. Enable INT8 quantization
3. Optimize batch size
4. Use TensorRT DLA (if available)

---

## Model Requirements

### TensorRT Engine Format
- File extension: `.trt` or `.engine`
- Convert từ ONNX/PyTorch/TensorFlow
- Optimized for specific GPU

### Model Paths
- Vehicle: `./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt`
- Plate Detection: `./cvedix_data/models/trt/plate/det_v8.5.trt`
- Plate Recognition: `./cvedix_data/models/trt/plate/rec_v8.5.trt`
- YOLOv8: `./cvedix_data/models/trt/yolov8/yolov8n.trt`

---

## Use Cases

### 1. High-Performance Detection
```bash
# Vehicle and plate detection
./build/bin/trt_infer_sample
```

### 2. General Object Detection
```bash
# YOLOv8 detection
./build/bin/trt_yolov8_sample
```

### 3. Parallel Processing
```bash
# Multiple models
./build/bin/multi_trt_infer_nodes_sample
```

---

## Troubleshooting

### Issue: TensorRT engine load failed
- Check engine file path
- Verify TensorRT version
- Check CUDA compatibility

### Issue: Out of memory
- Reduce batch size
- Use smaller model
- Free GPU memory

### Issue: Slow performance
- Check GPU utilization
- Verify FP16/INT8 enabled
- Optimize engine build

---

## Related Documentation

- [InsightFace TensorRT](README_INSIGHTFACE_TRT.md) - Face recognition
- [Main README](README.md) - Tổng quan samples


