# InsightFace ONNX Samples

## Tổng quan

Các samples này sử dụng **ONNX models** với **OpenCV DNN backend**, không cần TensorRT. Phù hợp cho:
- Development và testing
- Deployment trên CPU hoặc GPU không có TensorRT
- Prototyping nhanh

---

## Sample 1: Basic Recognition

### File
`insightface_sample.cpp`

### Mô tả
Sample đơn giản nhất để demo InsightFace face recognition với ONNX model:
- Phát hiện khuôn mặt (YuNet)
- Trích xuất embeddings (InsightFace ONNX)
- Hiển thị kết quả (OSD + Screen)

### Pipeline
```
Video File → YuNet Detector → InsightFace Recognition → OSD → Screen
```

### Build
```bash
cd build
cmake ..
make insightface_sample
```

### Usage

```bash
# Với default paths
./build/bin/insightface_sample

# Với custom paths
./build/bin/insightface_sample \
    ./cvedix_data/test_video/face.mp4 \
    ./cvedix_data/models/face/face_recognition_sface_2021dec.onnx
```

### Arguments
- `[video_path]` (optional): Đường dẫn đến video file (default: `./cvedix_data/test_video/face.mp4`)
- `[onnx_model_path]` (optional): Đường dẫn đến ONNX model (default: `./cvedix_data/models/face/face_recognition_sface_2021dec.onnx`)

### Expected Output

```
========================================
InsightFace ONNX Recognition Sample
========================================
Configuration:
  Video: ./cvedix_data/test_video/face.mp4
  Model: ./cvedix_data/models/face/face_recognition_sface_2021dec.onnx
  Alignment: Enabled (5-point landmarks)
  Embedding: Auto-detected from model
  Backend: OpenCV DNN (ONNX Runtime)
========================================

Pipeline: file_src → detector → recognizer → osd → screen

Starting pipeline...
Press ENTER to stop

[INFO] [recognizer] Loaded ONNX model: .../face_recognition_sface_2021dec.onnx (embedding_size=512)
[INFO] [recognizer] Detected embedding size: 512
```

### Features
- ✅ Face detection với YuNet
- ✅ Face alignment (5-point landmarks)
- ✅ Embedding extraction (512-dim, L2-normalized)
- ✅ Real-time visualization
- ✅ Performance monitoring (Analysis Board)

### Customization

#### Disable face alignment
```cpp
auto recognizer = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>(
    "recognizer",
    onnx_model_path,
    112, 112,
    false  // disable alignment
);
```

#### Adjust detection threshold
```cpp
auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
    "detector", 
    "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    0.7f,   // Lower threshold = more detections
    0.3f,
    5000
);
```

---

## Sample 2: Register & Recognize

### File
`insightface_register_recognize_face_sample.cpp`

### Mô tả
Sample đầy đủ với 2 chế độ:
1. **Register**: Đăng ký khuôn mặt từ ảnh vào database
2. **Recognize**: Nhận diện khuôn mặt trong video/ảnh

### Features
- ✅ Face registration từ ảnh
- ✅ Face recognition trong video/ảnh
- ✅ Database management (load/save)
- ✅ Cosine similarity matching
- ✅ Threshold-based identification

### Build
```bash
cd build
cmake ..
make insightface_register_recognize_face_sample
```

### Usage

#### Mode 1: Register (Đăng ký)

```bash
# Đăng ký một người từ ảnh
./build/bin/insightface_register_recognize_face_sample register \
    ./cvedix_data/test_images/faces/alice.jpg \
    "Alice"

# Với custom ONNX model
./build/bin/insightface_register_recognize_face_sample register \
    ./cvedix_data/test_images/faces/alice.jpg \
    "Alice" \
    ./cvedix_data/models/face/face_recognition_sface_2021dec.onnx
```

**Output:**
```
=== Face Registration Mode ===
[DB] Creating new database
[Register] Image: ./cvedix_data/test_images/faces/alice.jpg, Name: Alice
[Register] Face: (120,80) 150x180 (score: 0.987)
[DB] Saved 1 faces
[Register] ✓ Registered: Alice

[DB] Registered (1):
  - Alice

✓ Registration completed!
```

#### Mode 2: Recognize (Nhận diện)

```bash
# Nhận diện trong video
./build/bin/insightface_register_recognize_face_sample recognize \
    ./cvedix_data/test_video/face.mp4

# Nhận diện trong ảnh
./build/bin/insightface_register_recognize_face_sample recognize \
    ./cvedix_data/test_images/faces/test.jpg

# Với custom ONNX model
./build/bin/insightface_register_recognize_face_sample recognize \
    ./cvedix_data/test_video/face.mp4 \
    ./cvedix_data/models/face/face_recognition_sface_2021dec.onnx
```

**Output (Video mode):**
```
=== Face Recognition Mode ===
[DB] Loaded 1 faces

[DB] Registered (1):
  - Alice

Config: Input=./cvedix_data/test_video/face.mp4, Type=Video, DB=1 faces, Threshold=0.6

Pipeline: file_src → detector → recognizer → osd → screen
Starting... Press ENTER to stop

[Recognition] (245,156) -> Alice (0.89)
[Recognition] (512,178) -> Unknown
```

**Output (Image mode):**
```
=== Face Recognition Mode ===
[Image Recognition] Processing: ./cvedix_data/test_images/faces/test.jpg

[Results]
  Face 1: (120,80) 150x180 -> Alice (0.92)
  Face 2: (350,100) 140x170 -> Unknown

[Output] Result saved to: recognition_result.jpg
```

### Database Format

Database được lưu trong file `face_database.txt`:

```
Alice|0.123456,-0.567890,0.901234,...
Bob|0.234567,-0.678901,0.012345,...
```

**Format:** `name|embedding1,embedding2,embedding3,...`

### Database Location

- Default: `./face_database.txt` (trong thư mục chạy)
- Tự động resolve path từ executable location
- Tự động tạo nếu chưa tồn tại

### Similarity Threshold

Mặc định: **0.6**

Để thay đổi:
```cpp
g_database->set_threshold(0.7f);  // Higher = stricter matching
```

**Recommendations:**
- **0.5-0.6**: Lenient (more matches, possible false positives)
- **0.6-0.7**: Balanced (recommended)
- **0.7-0.8**: Strict (fewer matches, higher accuracy)

---

## Model Preparation

### 1. Download ONNX Model

```bash
# Option 1: Download từ InsightFace
cd cvedix_data/models/face
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip
unzip buffalo_l.zip
# Model: buffalo_l/w600k_r50.onnx

# Option 2: Sử dụng model có sẵn
# face_recognition_sface_2021dec.onnx (512-dim)
```

### 2. Download Face Detector

```bash
cd cvedix_data/models/face
wget https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2022mar.onnx
```

### 3. Verify Models

```bash
# Check file exists
ls -lh cvedix_data/models/face/*.onnx

# Expected:
# - face_detection_yunet_2022mar.onnx (~1.2MB)
# - face_recognition_sface_2021dec.onnx (~100MB)
```

---

## Performance

### CPU Performance
- **Detection**: ~15-30ms per frame (depends on resolution)
- **Recognition**: ~20-40ms per face (depends on model)
- **Total**: ~35-70ms per face

### GPU Performance (CUDA)
- **Detection**: ~5-10ms per frame
- **Recognition**: ~5-15ms per face
- **Total**: ~10-25ms per face

### Optimization Tips

1. **Use GPU if available**
   ```bash
   # Build with CUDA
   cmake -DCVEDIX_WITH_CUDA=ON ..
   ```

2. **Reduce input resolution**
   ```cpp
   // Smaller input = faster inference
   auto recognizer = std::make_shared<...>(
       "recognizer", model_path,
       96, 96,  // Instead of 112, 112
       true
   );
   ```

3. **Disable alignment if not needed**
   ```cpp
   enable_alignment = false;  // Faster but may reduce accuracy
   ```

---

## Troubleshooting

### Issue 1: Model không load

```bash
# Check file exists
ls -lh ./cvedix_data/models/face/face_recognition_sface_2021dec.onnx

# Check permissions
chmod 644 ./cvedix_data/models/face/*.onnx

# Verify ONNX file
python3 -c "import onnx; onnx.load('face_recognition_sface_2021dec.onnx')"
```

### Issue 2: No faces detected

```bash
# Lower detection threshold
# Edit code hoặc check video có faces không

# Test detector separately
python3 -c "
import cv2
detector = cv2.FaceDetectorYN.create('face_detection_yunet_2022mar.onnx', '', (320, 320))
img = cv2.imread('test.jpg')
detector.setInputSize(img.shape[:2][::-1])
faces, _ = detector.detect(img)
print(f'Detected {len(faces)} faces')
"
```

### Issue 3: Embedding size mismatch

```bash
# Check model output
# Sample sẽ auto-detect embedding size
# Nếu lỗi, check log:
[INFO] [recognizer] Detected embedding size: 512
```

### Issue 4: Database không load

```bash
# Check database file format
cat face_database.txt

# Format phải đúng:
# name|embedding1,embedding2,embedding3,...
# Không có spaces, không có empty lines
```

---

## Code Examples

### Example 1: Custom hook để log embeddings

```cpp
auto embedding_hooker = [](std::string node_name, int queue_size,
                            std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm) return;
    
    for (auto& face : fm->face_targets) {
        if (!face->embeddings.empty()) {
            std::cout << "Face embedding (" << face->embeddings.size() << " dim): ";
            for (size_t i = 0; i < std::min(10UL, face->embeddings.size()); i++) {
                std::cout << face->embeddings[i] << " ";
            }
            std::cout << "..." << std::endl;
        }
    }
};

recognizer->set_meta_handled_hooker(embedding_hooker);
```

### Example 2: Save detected faces

```cpp
auto save_faces_hooker = [](std::string node_name, int queue_size,
                            std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    static int counter = 0;
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm) return;
    
    for (auto& face : fm->face_targets) {
        cv::Rect roi(face->x, face->y, face->width, face->height);
        cv::Mat face_crop = fm->frame(roi);
        cv::imwrite("face_" + std::to_string(counter++) + ".jpg", face_crop);
    }
};

detector->set_meta_handled_hooker(save_faces_hooker);
```

### Example 3: Real-time database matching

```cpp
// Load database
auto db = std::make_shared<FaceDatabase>(argv[0]);

// Hook để match real-time
auto match_hooker = [db](std::string node_name, int queue_size,
                         std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm || !db) return;
    
    for (auto& face : fm->face_targets) {
        if (!face->embeddings.empty()) {
            std::string name = db->identify(face->embeddings);
            std::cout << "Matched: " << name << std::endl;
            
            // Add custom field
            face->custom_data["person_name"] = name;
        }
    }
};

recognizer->set_meta_handled_hooker(match_hooker);
```

---

## Integration với Pipeline khác

### Add RTMP output

```cpp
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"

auto rtmp = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
    "rtmp", 0, "rtmp://server/live/stream"
);

// Connect
auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", false);
osd->attach_to({recognizer});
split->attach_to({osd});
screen->attach_to({split});
rtmp->attach_to({split});
```

### Add MQTT publishing

```cpp
#include "cvedix/nodes/broker/cvedix_mqtt_json_broker_node.h"

auto mqtt = std::make_shared<cvedix_nodes::cvedix_mqtt_json_broker_node>(
    "mqtt", "tcp://broker:1883", "face_recognition"
);

recognizer->attach_to({detector});
mqtt->attach_to({recognizer});
osd->attach_to({recognizer});
```

---

## Best Practices

1. **Always use face alignment** cho accuracy tốt nhất
2. **Normalize embeddings** (L2 normalization) - đã tự động
3. **Use appropriate threshold** (0.6-0.7 recommended)
4. **Monitor performance** với Analysis Board
5. **Test với diverse dataset** trước khi deploy

---

## Next Steps

1. ✅ Test basic sample với video của bạn
2. ✅ Register một vài faces vào database
3. ✅ Test recognition với video/ảnh
4. ✅ Tune threshold cho use case của bạn
5. ✅ Integrate vào production pipeline

---

## Related Documentation

- [Main README](README_INSIGHTFACE.md) - Tổng quan tất cả samples
- [TensorRT Samples](README_INSIGHTFACE_TRT.md) - TensorRT version
- [Build Guide](../doc/BUILD_GUIDE_INSIGHTFACE.md)
- [Face Recognition Design](../doc/FACE_RECOGNITION_INSIGHTFACE.md)

