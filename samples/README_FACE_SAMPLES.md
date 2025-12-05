# Face Processing Samples

## Tổng quan

Các samples này demo các chức năng xử lý khuôn mặt: detection, tracking, recognition, và face swap.

---

## Sample 1: Face Tracking

### File
`face_tracking_sample.cpp`

### Mô tả
Tracking nhiều khuôn mặt trong video sử dụng SORT algorithm.

### Pipeline
```
Video → Face Detector → Face Encoder → SORT Tracker → OSD → Screen
```

### Features
- Face detection với YuNet
- Face feature encoding (SFace)
- Multi-face tracking với SORT
- Track ID persistence across frames

### Usage
```bash
./build/bin/face_tracking_sample
```

### Expected Output
- Video window với bounding boxes
- Track IDs hiển thị trên mỗi face
- Track IDs giữ nguyên khi face di chuyển

### Customization

#### Thay đổi tracking parameters
```cpp
auto tracker = std::make_shared<cvedix_sort_track_node>(
    "tracker",
    cvedix_track_for::FACE,
    max_age,      // Max frames to keep lost tracks
    min_hits,     // Min hits to confirm track
    iou_threshold // IoU threshold for matching
);
```

---

## Sample 2: Face Tracking với RTSP

### File
`face_tracking_rtsp_sample.cpp`

### Mô tả
Tương tự face_tracking_sample nhưng sử dụng RTSP stream làm input.

### Pipeline
```
RTSP Stream → Face Detector → Face Encoder → Tracker → OSD → Screen
```

### Features
- RTSP input source
- Real-time face tracking
- Support stream switching/restarting

### Usage
```bash
./build/bin/face_tracking_rtsp_sample
```

### Configuration
```cpp
auto rtsp_src = std::make_shared<cvedix_rtsp_src_node>(
    "rtsp_src",
    0,
    "rtsp://camera_ip:554/stream",  // RTSP URL
    0.6  // playback speed
);
```

---

## Sample 3: Face Swap

### File
`face_swap_sample.cpp`

### Mô tả
Swap khuôn mặt trong video với khuôn mặt từ source image.

### Pipeline
```
Video → Face Detector → Face Swap → OSD → Screen
```

### Features
- Face detection và alignment
- Face encoding và swapping
- Real-time face replacement

### Usage
```bash
./build/bin/face_swap_sample
```

### Requirements
- Face swap model (ONNX)
- Source face image
- Target video

### Configuration
```cpp
auto face_swap = std::make_shared<cvedix_face_swap_node>(
    "face_swap",
    swap_source_image,      // Path to source face image
    swap_source_face_index, // Which face in source image
    emap_file_path,         // Embedding map file
    face_extract_model,     // Face extraction model
    face_encoding_model,    // Face encoding model
    face_swap_model         // Face swap model
);
```

---

## Sample 4: YuNet INT8 Face Detection

### File
`face_yunet_int8_sample.cpp`

### Mô tả
Face detection sử dụng YuNet model INT8 quantized cho performance tốt hơn.

### Pipeline
```
Video → YuNet INT8 Detector → OSD → Screen
```

### Features
- INT8 quantized model (nhỏ hơn, nhanh hơn)
- Lower memory usage
- Faster inference trên CPU

### Usage
```bash
./build/bin/face_yunet_int8_sample
```

### Model Requirements
- `face_detection_yunet_2023mar_int8.onnx` (INT8 quantized)

### Performance
- **FP32**: ~15-20ms per frame
- **INT8**: ~8-12ms per frame (2x faster)

---

## Sample 5: InsightFace Recognition

### Files
- `insightface_sample.cpp` - ONNX version
- `insightface_trt_sample.cpp` - TensorRT version
- `insightface_register_recognize_face_sample.cpp` - ONNX register + recognize
- `insightface_register_recognize_face_trt_sample.cpp` - TensorRT register + recognize

### Mô tả
Face recognition sử dụng InsightFace models (ArcFace).

📖 Xem chi tiết: [README_INSIGHTFACE.md](README_INSIGHTFACE.md)

---

## So sánh các Face Samples

| Sample | Chức năng | Model | Performance |
|--------|-----------|-------|-------------|
| **face_tracking** | Detection + Tracking | YuNet + SFace | Good |
| **face_tracking_rtsp** | Tracking với RTSP | YuNet + SFace | Good |
| **face_swap** | Face replacement | Custom ONNX | Medium |
| **face_yunet_int8** | Fast detection | YuNet INT8 | Excellent |
| **insightface_*** | Recognition | InsightFace | Excellent |

---

## Build

```bash
cd build
cmake ..
make face_tracking_sample
make face_tracking_rtsp_sample
make face_swap_sample
make face_yunet_int8_sample
```

---

## Model Requirements

### Face Detection
- `face_detection_yunet_2022mar.onnx` (FP32)
- `face_detection_yunet_2023mar_int8.onnx` (INT8)

### Face Recognition
- `face_recognition_sface_2021dec.onnx` (SFace)
- `face_recognition_sface_2021dec.onnx` (InsightFace ONNX)
- `w600k_mbf_fp16_trt10.9.engine` (InsightFace TensorRT)

### Face Swap
- Face extraction model
- Face encoding model
- Face swap model
- Embedding map file

---

## Use Cases

### 1. Real-time Face Tracking
```bash
# Track faces in video
./build/bin/face_tracking_sample
```

### 2. Multi-camera Face Tracking
```bash
# Track faces from RTSP stream
./build/bin/face_tracking_rtsp_sample
```

### 3. Face Recognition System
```bash
# Register faces
./build/bin/insightface_register_recognize_face_sample register alice.jpg "Alice"

# Recognize in video
./build/bin/insightface_register_recognize_face_sample recognize video.mp4
```

### 4. Fast Detection (CPU)
```bash
# Use INT8 model for faster CPU inference
./build/bin/face_yunet_int8_sample
```

---

## Performance Tips

1. **Use INT8 model** cho CPU inference
2. **Use TensorRT** cho GPU inference
3. **Adjust detection threshold** để balance accuracy/speed
4. **Disable alignment** nếu không cần thiết (faster)

---

## Troubleshooting

### Issue: No faces detected
- Lower detection threshold
- Check video quality
- Verify model path

### Issue: Tracking lost
- Adjust tracking parameters (max_age, min_hits)
- Improve detection quality
- Use higher resolution input

### Issue: Slow performance
- Use INT8 model
- Reduce input resolution
- Use GPU acceleration

---

## Related Documentation

- [InsightFace Samples](README_INSIGHTFACE.md) - Face recognition
- [Main README](README.md) - Tổng quan samples

