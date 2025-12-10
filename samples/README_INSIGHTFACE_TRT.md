# InsightFace TensorRT Samples

## Tổng quan

Thư mục này chứa các samples demo sử dụng `cvedix_trt_insight_face_recognition_node` với TensorRT backend:

1. **insightface_trt_sample** - Basic pipeline (recommended để bắt đầu)
2. **insightface_register_recognize_face_trt_sample** - Register + Recognize với database

---

## Build Samples

### Requirements

- ✅ Build với `-DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON`
- ✅ Build với `-DCVEDIX_BUILD_SAMPLES=ON`
- ✅ TensorRT engine file đã prepared
- ✅ Test video file

### Build Commands

```bash
cd /home/cvedix/core_ai_runtime/build

# Configure
cmake -DCVEDIX_WITH_CUDA=ON \
      -DCVEDIX_WITH_TRT=ON \
      -DCVEDIX_BUILD_SAMPLES=ON \
      ..

# Build all samples
make -j8

# Hoặc build specific sample
make insightface_trt_sample
make insightface_register_recognize_face_trt_sample
```

### Verify Build

```bash
ls -lh build/bin/ | grep insightface
# Output:
# -rwxrwxr-x insightface_trt_sample
# -rwxrwxr-x insightface_register_recognize_face_trt_sample
```

---

## Sample 1: Basic Recognition Sample

### File
`insightface_trt_sample.cpp`

### Description
Pipeline đơn giản với InsightFace TensorRT:
- Video → Face Detector → Recognition → OSD → Display
- Face alignment với 5-point landmarks
- Real-time visualization
- Performance monitoring

### Usage

```bash
cd /home/cvedix/core_ai_runtime

# Set library path
export LD_LIBRARY_PATH=./build/libs:$LD_LIBRARY_PATH

# Run với default paths
./build/bin/insightface_trt_sample

# Run với custom paths
./build/bin/insightface_trt_sample \
    ./cvedix_data/test_video/face.mp4 \
    ./cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine
```

### Expected Output

```
========================================
InsightFace TensorRT Recognition Sample
========================================
Configuration:
Video: ./cvedix_data/test_video/face.mp4
  Model: ./cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine
  Alignment: Enabled (5-point landmarks)
  Embedding: 512-dim L2-normalized

Pipeline: file_src → detector → recognizer → osd → screen

Starting pipeline...
Press ENTER to stop

[INFO] [recognizer] Loaded ONNX model: .../w600k_mbf_fp16_trt10.9.engine (embedding_size=512)
[INFO] Node:detector, prepare:2ms, infer:8ms, post:1ms, total:11ms
[INFO] Node:recognizer, prepare:3ms, infer:10ms, post:0ms, total:13ms
```

### Visual Output
- Video window hiển thị với bounding boxes quanh faces
- Embeddings được extract tự động và L2-normalized
- Performance metrics hiển thị trên Analysis Board

---

## Sample 2: Register & Recognize Sample

### File
`insightface_register_recognize_face_trt_sample.cpp`

### Description
Sample đầy đủ với 2 chế độ:
1. **Register**: Đăng ký khuôn mặt từ ảnh vào database
2. **Recognize**: Nhận diện khuôn mặt trong video/ảnh

### Features
- ✅ Face registration từ ảnh
- ✅ Face recognition trong video/ảnh
- ✅ Database management (load/save)
- ✅ Cosine similarity matching
- ✅ Threshold-based identification

### Usage

#### Mode 1: Register (Đăng ký)

```bash
# Đăng ký một người từ ảnh
./build/bin/insightface_register_recognize_face_trt_sample register \
    ./cvedix_data/test_images/faces/alice.jpg \
    "Alice"

# Với custom TensorRT engine
./build/bin/insightface_register_recognize_face_trt_sample register \
    ./cvedix_data/test_images/faces/alice.jpg \
    "Alice" \
    ./cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine
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
./build/bin/insightface_register_recognize_face_trt_sample recognize \
    ./cvedix_data/test_video/face.mp4

# Nhận diện trong ảnh
./build/bin/insightface_register_recognize_face_trt_sample recognize \
    ./cvedix_data/test_images/faces/test.jpg

# Với custom TensorRT engine
./build/bin/insightface_register_recognize_face_trt_sample recognize \
    ./cvedix_data/test_video/face.mp4 \
    ./cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine
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

---

## Chuẩn bị Test Data

### 1. Download test video

```bash
cd /home/cvedix/core_ai_runtime

# Option A: Use existing test data
# Download cvedix_data từ:
# - Google Drive: https://drive.google.com/drive/folders/...
# - Baidu: https://pan.baidu.com/s/...

# Option B: Use your own video
mkdir -p cvedix_data/test_video
cp /path/to/your/video.mp4 cvedix_data/test_video/face.mp4
```

### 2. Prepare TensorRT engine

```bash
# Download ONNX model
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip
unzip buffalo_l.zip

# Convert to TensorRT
/usr/local/tensorRT/bin/trtexec \
  --onnx=buffalo_l/w600k_r50.onnx \
  --saveEngine=arcface_r50_fp16.engine \
  --fp16 \
  --memPoolSize=workspace:4096 \
  --shapes=data:1x3x112x112 \
  --minShapes=data:1x3x112x112 \
  --optShapes=data:8x3x112x112 \
  --maxShapes=data:16x3x112x112

# Deploy
mkdir -p cvedix_data/models/face
mv arcface_r50_fp16.engine cvedix_data/models/face/
```

### 3. Download face detection model (YuNet)

```bash
# Usually included in cvedix_data package
# Or download from OpenCV Model Zoo
cd cvedix_data/models/face
wget https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
mv face_detection_yunet_2023mar.onnx face_detection_yunet_2022mar.onnx
```

---

## Run Examples

### Scenario 1: Test với default video

```bash
cd /home/cvedix/core_ai_runtime
export LD_LIBRARY_PATH=./build/libs:$LD_LIBRARY_PATH

# Simple sample
./build/bin/insightface_trt_simple_sample

# Advanced sample với logging
./build/bin/insightface_trt_sample
```

### Scenario 2: Test với custom video

```bash
# Your own video file
./build/bin/insightface_trt_simple_sample \
    /path/to/your/video.mp4 \
    ./cvedix_data/models/face/arcface_r50_fp16.engine
```

### Scenario 3: Test với RTSP stream

Modify source trong code:

```cpp
// Replace cvedix_file_src_node với cvedix_rtsp_src_node
auto rtsp_src = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
    "rtsp_src", 
    0, 
    "rtsp://your_camera_ip:554/stream"
);
```

---

## Customization

### 1. Adjust confidence threshold

```cpp
// Lower threshold = more faces detected (more false positives)
auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
    "detector", 
    "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    0.7f,   // score_threshold (default: 0.9, lower = more detections)
    0.3f,   // nms_threshold
    5000    // top_k
);
```

### 2. Disable face alignment

```cpp
// Nếu faces đã aligned hoặc muốn faster inference
auto recognizer = std::make_shared<cvedix_trt_insight_face_recognition_node>(
    "recognizer",
    engine_path,
    112, 112,
    false  // disable alignment (faster but may reduce accuracy)
);
```

### 3. Add multiple outputs

```cpp
// Split để có nhiều outputs
auto split = std::make_shared<cvedix_split_node>("split", false);

auto screen = std::make_shared<cvedix_screen_des_node>("screen", 0);
auto rtmp = std::make_shared<cvedix_rtmp_des_node>(
    "rtmp", 0, "rtmp://server/live/stream"
);

// Connect
osd->attach_to({recognizer});
split->attach_to({osd});
screen->attach_to({split});
rtmp->attach_to({split});
```

### 4. Add custom processing

```cpp
// Lambda function để process embeddings
auto custom_hooker = [](std::string node_name, int queue_size, 
                        std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm) return;
    
    for (auto& face : fm->face_targets) {
        if (!face->embeddings.empty()) {
            // Your custom logic here
            // - Compare với database
            // - Log to file
            // - Send to API
            // - etc.
        }
    }
};

recognizer->set_meta_handled_hooker(custom_hooker);
```

---

## Performance Tips

### 1. Monitor FPS

Analysis board tự động hiển thị:
- **FPS per node**: Để identify bottlenecks
- **Queue size**: Detect buffer overflow
- **Latency**: Processing time per node

### 2. GPU Memory Usage

```bash
# Monitor GPU while running
watch -n 1 nvidia-smi

# Expected usage:
# - ResNet50 FP16: ~400-600MB
# - MobileFaceNet FP16: ~200-300MB
```

### 3. Optimize Batch Size

```cpp
// Edit: third_party/trt_insightface/models/insight_face_recognition.h
static constexpr int kBatchSize = 8;  // Adjust based on GPU

// Recommendations:
// RTX 4090: 16
// RTX 3080: 8-12
// RTX 2060: 4-6
// GTX 1080: 4
```

---

## Troubleshooting

### Issue 1: Sample không build

```bash
# Verify SAMPLES flag
cmake -DCVEDIX_BUILD_SAMPLES=ON ..

# Clean và rebuild
make clean
make insightface_trt_simple_sample
```

### Issue 2: Runtime error "Cannot load engine"

```bash
# Check file exists
ls -lh ./cvedix_data/models/face/arcface_r50_fp16.engine

# Check permissions
chmod 644 ./cvedix_data/models/face/arcface_r50_fp16.engine

# Verify path in code
./build/bin/insightface_trt_simple_sample \
    ./cvedix_data/test_video/face.mp4 \
    ./cvedix_data/models/face/arcface_r50_fp16.engine  # Full path
```

### Issue 3: No video display

```bash
# Check DISPLAY environment
echo $DISPLAY

# If running SSH, setup X11 forwarding
export DISPLAY=:0

# Or use VcXsrv on Windows
export DISPLAY=<your_windows_ip>:0.0
```

### Issue 4: No faces detected

```bash
# Lower detection threshold
# Edit code:
auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
    "detector", 
    "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    0.5f,   // Lower from 0.9 to 0.5
    0.3f,
    5000
);

# Rebuild và run lại
```

---

## Code Snippets

### Add RTMP output

```cpp
// Thêm sau screen node
auto rtmp_des = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
    "rtmp_des",
    0,                                      // channel index
    "rtmp://your_server:1935/live/camera", // RTMP URL
    cvedix_objects::cvedix_size{1280, 720}, // output size
    2048                                    // bitrate (kbps)
);

// Connect to pipeline (after split)
rtmp_des->attach_to({split});
```

### Save face crops to file

```cpp
// Hook để save detected faces
auto save_faces_hooker = [](std::string node_name, int queue_size,
                            std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    static int face_counter = 0;
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm) return;
    
    for (auto& face : fm->face_targets) {
        // Crop face from frame
        cv::Rect roi(face->x, face->y, face->width, face->height);
        cv::Mat face_crop = fm->frame(roi);
        
        // Save to file
        std::string filename = "./faces/face_" + 
                              std::to_string(face_counter++) + ".jpg";
        cv::imwrite(filename, face_crop);
    }
};

detector->set_meta_handled_hooker(save_faces_hooker);
```

### Calculate embedding statistics

```cpp
auto stats_hooker = [](std::string node_name, int queue_size,
                       std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm) return;
    
    for (auto& face : fm->face_targets) {
        if (face->embeddings.empty()) continue;
        
        // Calculate statistics
        float mean = 0.0f, std_dev = 0.0f;
        for (float val : face->embeddings) {
            mean += val;
        }
        mean /= face->embeddings.size();
        
        for (float val : face->embeddings) {
            std_dev += (val - mean) * (val - mean);
        }
        std_dev = std::sqrt(std_dev / face->embeddings.size());
        
        std::cout << "Embedding stats: mean=" << mean 
                  << ", std=" << std_dev << std::endl;
    }
};

recognizer->set_meta_handled_hooker(stats_hooker);
```

---

## Integration Examples

### Use with broker

```cpp
#include "cvedix/nodes/broker/cvedix_embeddings_socket_broker_node.h"

auto broker = std::make_shared<cvedix_embeddings_socket_broker_node>(
    "broker",
    cvedix_broke_for::FACE,  // broker for face embeddings
    "127.0.0.1",              // destination IP
    8888                      // destination port
);

// Connect after recognizer
recognizer->attach_to({detector});
broker->attach_to({recognizer});
osd->attach_to({recognizer});  // parallel branch
```

### Multi-camera setup

```cpp
// 2 cameras sharing same detector and recognizer
auto src_0 = std::make_shared<cvedix_file_src_node>("src_0", 0, "cam1.mp4");
auto src_1 = std::make_shared<cvedix_file_src_node>("src_1", 1, "cam2.mp4");

// Shared nodes (multi-channel support)
auto detector = std::make_shared<cvedix_yunet_face_detector_node>(...);
auto recognizer = std::make_shared<cvedix_trt_insight_face_recognition_node>(...);

// Split outputs by channel
auto split = std::make_shared<cvedix_split_node>("split", true);  // by channel

auto screen_0 = std::make_shared<cvedix_screen_des_node>("screen_0", 0);
auto screen_1 = std::make_shared<cvedix_screen_des_node>("screen_1", 1);

// Pipeline
detector->attach_to({src_0, src_1});
recognizer->attach_to({detector});
split->attach_to({recognizer});
screen_0->attach_to({split});
screen_1->attach_to({split});

src_0->start();
src_1->start();
```

---

## Files in This Directory

```
samples/
├── insightface_trt_sample.cpp                          # Basic sample (recommended)
├── insightface_register_recognize_face_trt_sample.cpp  # Register + Recognize
└── README_INSIGHTFACE_TRT.md                          # This file
```

---

## Next Steps

1. ✅ Run basic sample để verify setup
2. ✅ Register faces vào database
3. ✅ Test recognition với video/ảnh
4. ✅ Monitor performance với analysis board
5. ✅ Tune threshold cho use case của bạn
6. ✅ Integrate vào production pipeline

---

## Related Documentation

- [Main README](README_INSIGHTFACE.md) - Tổng quan tất cả samples
- [ONNX Samples](README_INSIGHTFACE_ONNX.md) - ONNX version
- [Build Guide](../doc/BUILD_GUIDE_INSIGHTFACE.md) - Detailed build instructions
- [Build Errors Analysis](../doc/BUILD_ERRORS_ANALYSIS.md) - Troubleshooting
- [Face Recognition Design](../doc/FACE_RECOGNITION_INSIGHTFACE.md) - Architecture
- [Model Preparation](../third_party/trt_insightface/MODEL_PREPARATION.md) - Model guide

---

**Happy coding! 🚀**


