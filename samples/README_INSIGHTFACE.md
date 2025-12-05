# InsightFace Face Recognition Samples

## Tổng quan

Thư mục này chứa các samples demo sử dụng InsightFace cho face recognition, hỗ trợ cả **ONNX** và **TensorRT** backends.

### Danh sách Samples

| Sample | Backend | Mô tả | File |
|--------|---------|-------|------|
| **insightface_sample** | ONNX | Basic pipeline - Extract embeddings | `insightface_sample.cpp` |
| **insightface_register_recognize_face_sample** | ONNX | Register + Recognize faces | `insightface_register_recognize_face_sample.cpp` |
| **insightface_trt_sample** | TensorRT | Basic pipeline với TensorRT | `insightface_trt_sample.cpp` |
| **insightface_register_recognize_face_trt_sample** | TensorRT | Register + Recognize với TensorRT | `insightface_register_recognize_face_trt_sample.cpp` |

---

## Quick Start

### 1. ONNX Samples (Không cần TensorRT)

```bash
# Basic sample
./build/bin/insightface_sample \
    ./cvedix_data/test_video/face.mp4 \
    ./cvedix_data/models/face/face_recognition_sface_2021dec.onnx

# Register + Recognize
./build/bin/insightface_register_recognize_face_sample register alice.jpg "Alice"
./build/bin/insightface_register_recognize_face_sample recognize face.mp4
```

### 2. TensorRT Samples (Cần TensorRT)

```bash
# Basic sample
./build/bin/insightface_trt_sample \
    ./cvedix_data/test_video/face.mp4 \
    ./cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine

# Register + Recognize
./build/bin/insightface_register_recognize_face_trt_sample register alice.jpg "Alice"
./build/bin/insightface_register_recognize_face_trt_sample recognize face.mp4
```

---

## So sánh ONNX vs TensorRT

| Feature | ONNX | TensorRT |
|---------|------|----------|
| **Build requirement** | Không cần flag đặc biệt | `-DCVEDIX_WITH_TRT=ON` |
| **Model format** | `.onnx` file | `.engine` file |
| **Performance** | Tốt (CPU/CUDA) | Rất tốt (GPU optimized) |
| **Setup** | Đơn giản | Cần convert model |
| **Portability** | Cao | Phụ thuộc GPU |
| **Use case** | Development, testing | Production, high-performance |

---

## Chi tiết từng Sample

### 📄 [README_INSIGHTFACE_ONNX.md](README_INSIGHTFACE_ONNX.md)
Chi tiết về ONNX samples:
- `insightface_sample` - Basic recognition
- `insightface_register_recognize_face_sample` - Register & Recognize

### 📄 [README_INSIGHTFACE_TRT.md](README_INSIGHTFACE_TRT.md)
Chi tiết về TensorRT samples:
- `insightface_trt_sample` - Basic recognition
- `insightface_register_recognize_face_trt_sample` - Register & Recognize

---

## Build Requirements

### ONNX Samples
```bash
cmake ..
make insightface_sample
make insightface_register_recognize_face_sample
```

### TensorRT Samples
```bash
cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON ..
make insightface_trt_sample
make insightface_register_recognize_face_trt_sample
```

---

## Model Requirements

### ONNX Models
- `face_recognition_sface_2021dec.onnx` (512-dim)
- Download từ: https://github.com/deepinsight/insightface

### TensorRT Engines
- `w600k_mbf_fp16_trt10.9.engine`
- Convert từ ONNX bằng TensorRT

### Face Detector (Cả hai)
- `face_detection_yunet_2022mar.onnx`
- Download từ OpenCV Model Zoo

---

## Common Issues

### 1. Model not found
```bash
# Check model path
ls -lh ./cvedix_data/models/face/*.onnx
ls -lh ./cvedix_data/models/trt/face/*.engine
```

### 2. Library path
```bash
export LD_LIBRARY_PATH=./build/libs:$LD_LIBRARY_PATH
```

### 3. Display (for screen output)
```bash
export DISPLAY=:0
```

---

## Next Steps

1. ✅ Chọn backend phù hợp (ONNX hoặc TensorRT)
2. ✅ Xem chi tiết trong README tương ứng
3. ✅ Test với sample đơn giản trước
4. ✅ Thử register + recognize nếu cần
5. ✅ Tích hợp vào production pipeline

---

## Related Documentation

- [Build Guide](../doc/BUILD_GUIDE_INSIGHTFACE.md)
- [Face Recognition Design](../doc/FACE_RECOGNITION_INSIGHTFACE.md)
- [Model Preparation](../third_party/trt_insightface/MODEL_PREPARATION.md)


