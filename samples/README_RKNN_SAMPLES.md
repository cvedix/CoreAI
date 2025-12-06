# RKNN Samples

## Tổng quan

Các samples này demo sử dụng RKNN (Rockchip Neural Network) runtime để chạy inference trên Rockchip SoCs (RK3588, RK3568, etc.).

---

## Sample 1: RKNN Face Detector

### File
`rknn_face_detector_sample.cpp`

### Mô tả
Face detection sử dụng RKNN YOLOv8 model.

### Pipeline
```
Video → RKNN YOLOv8 Detector → Tracker → OSD → Screen
```

### Features
- RKNN model inference
- Face detection
- Multi-face tracking
- RGA acceleration (optional)

### Usage
```bash
./build/bin/rknn_face_detector_sample
```

### Requirements
- RKNN model (.rknn)
- librknnrt.so
- librga.so (optional, for acceleration)

### Build
```bash
cmake -DCVEDIX_WITH_RKNN=ON [-DCVEDIX_WITH_RGA=ON] ..
make rknn_face_detector_sample
```

### Configuration
```cpp
auto detector = std::make_shared<cvedix_rknn_yolov8_detector_node>(
    "detector",
    "./cvedix_data/models/face/yolov8n_face_detection.rknn",
    0.5,  // score threshold
    0.5,  // NMS threshold
    640,  // input width
    640,  // input height
    1     // num classes
);
```

---

## Sample 2: RKNN Face Detection (File)

### File
`rknn_face_detection_file_sample.cpp`

### Mô tả
Tương tự rknn_face_detector_sample nhưng với file input.

### Pipeline
```
File → RKNN Detector → OSD → Screen
```

### Usage
```bash
./build/bin/rknn_face_detection_file_sample
```

---

## Sample 3: RKNN Face Detection (Basic)

### File
`rknn_face_detection_sample.cpp`

### Mô tả
Basic face detection với RKNN.

### Pipeline
```
Video → RKNN Detector → OSD → Screen
```

### Usage
```bash
./build/bin/rknn_face_detection_sample
```

---

## Sample 4: RKNN Face Tracking

### File
`rknn_face_tracking_sample.cpp`

### Mô tả
Face detection và tracking với RKNN.

### Pipeline
```
Video → RKNN Detector → Tracker → OSD → Screen
```

### Features
- Face detection
- Multi-face tracking
- Track ID persistence

### Usage
```bash
./build/bin/rknn_face_tracking_sample
```

---

## Sample 5: RKNN RTSP Tracking

### File
`rknn_rtsp_tracking_sample.cpp`

### Mô tả
Face tracking từ RTSP stream với RKNN.

### Pipeline
```
RTSP → RKNN Detector → Tracker → Broker → OSD → [Screen, RTMP]
```

### Features
- RTSP input
- RKNN detection
- Tracking
- JSON broker output
- Auto codec detection

### Usage
```bash
./build/bin/rknn_rtsp_tracking_sample
```

### Configuration
```cpp
auto rtsp_src = std::make_shared<cvedix_rtsp_src_node>(
    "rtsp_src",
    0,
    "rtsp://admin:password@192.168.1.114:554/cam/realmonitor?channel=1&subtype=0",
    1.0,
    true,
    "auto"  // Auto-detect codec (H264/H265)
);
```

### Broker Output
- Base64 encoded full frame
- Base64 encoded crop images
- Bounding box coordinates
- Confidence scores
- Track IDs

---

## Sample 6: RKNN RTSP Tracking với MQTT

### File
`rknn_rtsp_tracking_mqtt_sample.cpp`

### Mô tả
RTSP tracking với MQTT broker output.

### Pipeline
```
RTSP → RKNN Detector → Tracker → MQTT Broker → OSD → Screen
```

### Features
- RTSP input
- RKNN detection
- MQTT messaging
- Real-time streaming

### Usage
```bash
./build/bin/rknn_rtsp_tracking_mqtt_sample
```

### Requirements
- Build với `-DCVEDIX_WITH_MQTT=ON`
- MQTT broker

---

## Sample 7: RKNN YOLOv11 Detector

### File
`rknn_yolov11_detector_sample.cpp`

### Mô tả
Object detection sử dụng RKNN YOLOv11 model.

### Pipeline
```
Video → RKNN YOLOv11 Detector → OSD → Screen
```

### Features
- YOLOv11 model
- Multi-class detection
- Custom labels support

### Usage
```bash
# With arguments
./build/bin/rknn_yolov11_detector_sample \
    ./models/yolov11s.rknn \
    ./test_video.mp4 \
    ./labels.txt

# Default paths
./build/bin/rknn_yolov11_detector_sample
```

### Arguments
1. Model path (.rknn file)
2. Video path
3. Labels path (optional)

### Example
```bash
./build/bin/rknn_yolov11_detector_sample \
    ./cvedix_data/models/rknn/rk3588/yolov11s.rknn \
    ./cvedix_data/test_video/plate.mp4 \
    ./cvedix_data/models/det_cls/coco_labels.txt
```

---

## So sánh các RKNN Samples

| Sample | Input | Model | Features |
|--------|-------|-------|----------|
| **rknn_face_detector** | Video | YOLOv8 | Face detection |
| **rknn_face_detection_file** | File | YOLOv8 | Face detection |
| **rknn_face_detection** | Video | Custom | Basic detection |
| **rknn_face_tracking** | Video | YOLOv8 | Detection + tracking |
| **rknn_rtsp_tracking** | RTSP | YOLOv8 | RTSP + tracking + broker |
| **rknn_rtsp_tracking_mqtt** | RTSP | YOLOv8 | RTSP + MQTT |
| **rknn_yolov11_detector** | Video | YOLOv11 | Multi-class detection |

---

## Build

### Basic Build
```bash
cd build
cmake -DCVEDIX_WITH_RKNN=ON ..
make
```

### With RGA Acceleration
```bash
cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_WITH_RGA=ON ..
make
```

### With MQTT Support
```bash
cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_WITH_MQTT=ON ..
make
```

---

## Model Requirements

### RKNN Model Format
- File extension: `.rknn`
- Convert từ ONNX/PyTorch/TensorFlow
- Use RKNN Toolkit for conversion

### Model Conversion
```bash
# Using RKNN Toolkit
python convert_to_rknn.py \
    --model yolov8n.onnx \
    --target rk3588 \
    --output yolov8n.rknn
```

### Model Paths
- Face detection: `./cvedix_data/models/face/yolov8n_face_detection.rknn`
- YOLOv11: `./cvedix_data/models/rknn/rk3588/yolov11s.rknn`

---

## Performance

### RK3588 Performance
- **Face Detection**: ~10-15ms per frame
- **YOLOv11 Detection**: ~15-25ms per frame
- **With RGA**: ~30-50% faster

### Optimization Tips
1. Use RGA for preprocessing
2. Optimize model quantization
3. Use appropriate input size
4. Batch processing when possible

---

## Hardware Requirements

### Supported SoCs
- RK3588 (recommended)
- RK3568
- RK3566
- RK3399Pro

### Libraries
- `librknnrt.so` (required)
- `librga.so` (optional, for acceleration)

### Installation
```bash
# Copy libraries to system path
sudo cp librknnrt.so /usr/lib/
sudo cp librga.so /usr/lib/
sudo ldconfig
```

---

## Use Cases

### 1. Edge Face Detection
```bash
# Detect faces on edge device
./build/bin/rknn_face_detector_sample
```

### 2. Real-time RTSP Processing
```bash
# Process RTSP stream
./build/bin/rknn_rtsp_tracking_sample
```

### 3. Multi-class Detection
```bash
# Detect multiple object classes
./build/bin/rknn_yolov11_detector_sample
```

### 4. IoT Integration
```bash
# MQTT integration
./build/bin/rknn_rtsp_tracking_mqtt_sample
```

---

## Troubleshooting

### Issue: Model load failed
- Check model path
- Verify model format (.rknn)
- Check RKNN runtime version

### Issue: Slow performance
- Enable RGA acceleration
- Reduce input resolution
- Use quantized model (INT8)

### Issue: RTSP connection failed
- Check network connectivity
- Verify RTSP URL
- Check codec support

### Issue: RGA not working
- Verify librga.so installed
- Check RGA version compatibility
- Disable RGA if not needed

---

## Related Documentation

- [Main README](README.md) - Tổng quan samples
- [Face Samples](README_FACE_SAMPLES.md) - Face processing




