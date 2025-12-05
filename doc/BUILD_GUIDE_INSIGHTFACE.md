# Hướng dẫn Build InsightFace Recognition Node

## Tổng quan

Tài liệu này hướng dẫn build và sử dụng node `cvedix_trt_insight_face_recognition_node` trong CVEDIX AI Runtime.

---

## 1. Phân tích các lỗi đã gặp và cách fix

### ❌ Lỗi 1: CMake Export Error

**Lỗi hiện tại**:
```
CMake Error: install(EXPORT "cvedix-targets" ...) includes target "cvedix_instance_sdk" 
which requires target "trt_insightface" that is not in any export set.
```

**Nguyên nhân**: 
- Thư viện `trt_insightface` được link vào `cvedix_instance_sdk`
- Nhưng không được export trong install targets
- CMake yêu cầu tất cả dependencies phải được export cùng

**Giải pháp** (ĐÃ FIX):
```cmake
# Trong CMakeLists.txt, thêm install với EXPORT
install(TARGETS trt_insightface EXPORT cvedix-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
```

---

### ❌ Lỗi 2: TensorRT API Incompatibility

**Lỗi hiện tại**:
```cpp
error: 'class nvinfer1::ICudaEngine' has no member named 'getNbBindings'
error: 'class nvinfer1::ICudaEngine' has no member named 'getBindingIndex'
error: 'class nvinfer1::IExecutionContext' has no member named 'enqueue'
```

**Nguyên nhân**: 
- Code được viết cho **TensorRT 8.x**
- Hệ thống đang dùng **TensorRT 10.x+** (với CUDA 12.8)
- TensorRT 10+ đã deprecated/removed nhiều API cũ

**API Changes**:

| TensorRT 8.x (Old) | TensorRT 10.x+ (New) |
|--------------------|----------------------|
| `engine->getNbBindings()` | Removed (use IOTensor API) |
| `engine->getBindingIndex(name)` | Removed (use `setTensorAddress()`) |
| `context->enqueue()` | `context->enqueueV3()` |
| Implicit batch dimension | **Explicit batch** (required) |

**Giải pháp** (ĐÃ FIX):

```cpp
// OLD CODE (TensorRT 8.x)
void* bindings[2] = {gpu_input_buffer, gpu_output_buffer};
context->enqueue(batch_size, bindings, stream, nullptr);

// NEW CODE (TensorRT 10.x+)
context->setTensorAddress(kInputTensorName, gpu_input_buffer);
context->setTensorAddress(kOutputTensorName, gpu_output_buffer);
context->setInputShape(kInputTensorName, nvinfer1::Dims4{batch_size, 3, 112, 112});
context->enqueueV3(stream);
```

---

### ❌ Lỗi 3: CUDA Header Not Found

**Lỗi hiện tại**:
```
fatal error: cuda_runtime_api.h: No such file or directory
```

**Nguyên nhân**: 
- CUDA include directories không được add vào main project
- Chỉ có trong third_party libraries

**Giải pháp** (ĐÃ FIX):
```cmake
# Trong CMakeLists.txt
if(CVEDIX_WITH_TRT)
    # Add CUDA include directories
    include_directories(/usr/local/cuda/include)
    include_directories(${CVEDIX_TRT_INC_PATH})
endif()
```

---

### ❌ Lỗi 4: TensorRT Vehicle/YOLOv8 Incompatibility

**Lỗi hiện tại**:
```cpp
error: 'nvinfer1::ResizeMode' has not been declared
error: 'class nvinfer1::IConvolutionLayer' has no member named 'setStride'
error: 'class nvinfer1::IBuilder' has no member named 'setMaxBatchSize'
```

**Nguyên nhân**: 
- Code trong `trt_vehicle` và `trt_yolov8` cũng viết cho TensorRT 8.x
- Cần update rất nhiều files (>10 files, >500 lines)

**Giải pháp tạm thời** (ĐÃ FIX):
```cmake
# Comment out trt_vehicle và trt_yolov8 trong CMakeLists.txt
# Chỉ build trt_insightface

# Exclude vehicle/yolov8 nodes from compilation
if(CVEDIX_WITH_TRT)
    list(FILTER NODES EXCLUDE REGEX ".*cvedix_trt_vehicle.*")
    list(FILTER NODES EXCLUDE REGEX ".*cvedix_trt_yolov8.*")
endif()
```

---

## 2. Build Instructions

### Bước 1: Chuẩn bị môi trường

```bash
# Verify CUDA
nvcc --version
nvidia-smi

# Verify TensorRT (nếu có lỗi, cài TensorRT 10+)
ls /usr/local/tensorRT/lib/libnvinfer.so
```

### Bước 2: Build project

```bash
cd /home/cvedix/core_ai_runtime

# Clean build directory
rm -rf build
mkdir build && cd build

# Configure với TensorRT enabled
cmake -DCVEDIX_WITH_CUDA=ON \
      -DCVEDIX_WITH_TRT=ON \
      ..

# Compile
make -j8
```

### Bước 3: Kiểm tra build thành công

```bash
# Check libraries
ls -lh libs/
# Output:
# - libcvedix_instance_sdk.so  (~38MB)
# - libtrt_insightface.so      (~1.5MB)
# - libtinyexpr.so

# Check samples
ls -lh samples/
# Output:
# - face_recognition_test      (~398KB)
# - tinyexpr_test

# Verify node symbols
nm -D libs/libcvedix_instance_sdk.so | grep insight_face
# Should show constructor, destructor, và methods
```

---

## 3. Chuẩn bị Model

### Download InsightFace Model

```bash
cd /home/cvedix/core_ai_runtime

# Download buffalo_l (ResNet50)
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip
unzip buffalo_l.zip
```

### Convert ONNX sang TensorRT Engine

```bash
# Check input tensor name trước
/usr/local/tensorRT/bin/trtexec \
  --onnx=buffalo_l/w600k_r50.onnx \
  --verbose 2>&1 | grep -A3 "Input\|Output"

# Convert với explicit batch và TensorRT 10.x format
/usr/local/tensorRT/bin/trtexec \
  --onnx=buffalo_l/w600k_r50.onnx \
  --saveEngine=arcface_r50_fp16.engine \
  --fp16 \
  --memPoolSize=workspace:4096 \
  --shapes=data:1x3x112x112 \
  --minShapes=data:1x3x112x112 \
  --optShapes=data:8x3x112x112 \
  --maxShapes=data:16x3x112x112 \
  --verbose

# NOTE: 
# - TensorRT 10+ sử dụng --shapes thay vì --inputIOFormats
# - Sử dụng --memPoolSize thay vì --workspace
# - Explicit batch dimension (batch là phần của shape)
```

### Deploy Model

```bash
mkdir -p ./cvedix_data/models/face
cp arcface_r50_fp16.engine ./cvedix_data/models/face/
chmod 644 ./cvedix_data/models/face/arcface_r50_fp16.engine
```

---

## 4. Sử dụng Node trong Code

### Example 1: Basic Face Recognition Pipeline

Tạo file `main.cpp`:

```cpp
#include <iostream>
#include <memory>
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_trt_insight_face_recognition_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

using namespace cvedix_nodes;

int main() {
    // 1. Video source
    auto src = std::make_shared<cvedix_file_src_node>(
        "file_src", 
        "./test_video.mp4",  // video path
        0                     // channel index
    );

    // 2. Face detector (YuNet)
    auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
        "face_detector",
        "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx",
        0.9f,    // confidence threshold
        0.3f,    // NMS threshold
        5000,    // top_k
        320,     // input width
        320      // input height
    );

    // 3. Face recognition (InsightFace TensorRT)
    auto recognizer = std::make_shared<cvedix_trt_insight_face_recognition_node>(
        "face_recognizer",
        "./cvedix_data/models/face/arcface_r50_fp16.engine",
        112,     // input width
        112,     // input height
        true     // enable face alignment (using 5-point landmarks)
    );

    // 4. OSD (visualization)
    auto osd = std::make_shared<cvedix_face_osd_node_v2>(
        "face_osd",
        "./cvedix_data/font/NotoSansCJKsc-Regular.otf"
    );

    // 5. Screen display
    auto screen = std::make_shared<cvedix_screen_des_node>(
        "screen",
        0  // channel index
    );

    // Connect pipeline
    src->attach_to(detector);
    detector->attach_to(recognizer);
    recognizer->attach_to(osd);
    osd->attach_to(screen);

    // Start pipeline
    std::cout << "Starting face recognition pipeline..." << std::endl;
    src->start();

    return 0;
}
```

### Example 2: Face Recognition với Database

```cpp
#include <map>
#include <cmath>
#include "cvedix/nodes/common/cvedix_meta_hookable.h"
#include "third_party/trt_insightface/util/algorithm_util.h"

// Face database class
class FaceDatabase {
private:
    std::map<std::string, std::vector<float>> database_;
    float threshold_ = 0.6f;

public:
    void register_person(const std::string& name, 
                        const std::vector<float>& embedding) {
        database_[name] = embedding;
        std::cout << "[DB] Registered: " << name << std::endl;
    }

    std::string identify(const std::vector<float>& query_embedding) {
        std::string best_match = "Unknown";
        float best_sim = threshold_;

        for (const auto& [name, db_emb] : database_) {
            float sim = trt_insightface::util::cosine_similarity(
                query_embedding, db_emb
            );

            if (sim > best_sim) {
                best_sim = sim;
                best_match = name;
            }
        }

        return best_match + " (" + std::to_string(best_sim) + ")";
    }

    size_t size() const { return database_.size(); }
};

// Hook để process embeddings
class FaceRecognitionHook : public cvedix_meta_hookable {
private:
    std::shared_ptr<FaceDatabase> db_;

public:
    FaceRecognitionHook(std::shared_ptr<FaceDatabase> db) : db_(db) {}

    void meta_handle_after(std::shared_ptr<cvedix_objects::cvedix_meta> meta) override {
        auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
        if (!frame_meta || frame_meta->face_targets.empty()) {
            return;
        }

        // Process each face
        for (auto& face : frame_meta->face_targets) {
            if (face->embeddings.empty()) {
                continue;
            }

            // Identify person
            std::string result = db_->identify(face->embeddings);
            std::cout << "[Recognition] Face at (" << face->x << "," << face->y 
                      << ") -> " << result << std::endl;
        }
    }
};

int main() {
    // Initialize database
    auto db = std::make_shared<FaceDatabase>();
    
    // TODO: Load known faces from file/database
    // db->register_person("Alice", alice_embedding);
    // db->register_person("Bob", bob_embedding);

    // Create pipeline...
    auto recognizer = std::make_shared<cvedix_trt_insight_face_recognition_node>(
        "recognizer",
        "./cvedix_data/models/face/arcface_r50_fp16.engine"
    );

    // Attach hook
    auto hook = std::make_shared<FaceRecognitionHook>(db);
    recognizer->attach_hook(hook);

    // ... connect pipeline & start
}
```

### Example 3: Compile user application

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.10)
project(my_face_app)

set(CMAKE_CXX_STANDARD 17)

# Find CVEDIX SDK
set(cvedix_DIR /home/cvedix/core_ai_runtime/build)
include_directories(
    /home/cvedix/core_ai_runtime
    /home/cvedix/core_ai_runtime/build
)

link_directories(/home/cvedix/core_ai_runtime/build/libs)

# Create executable
add_executable(my_face_app main.cpp)

# Link libraries
target_link_libraries(my_face_app 
    cvedix_instance_sdk
    trt_insightface
    tinyexpr
    opencv_core
    opencv_imgproc
    opencv_highgui
)
```

**Build commands**:
```bash
mkdir build && cd build
cmake ..
make

# Run
export LD_LIBRARY_PATH=/home/cvedix/core_ai_runtime/build/libs:$LD_LIBRARY_PATH
./my_face_app
```

---

## 5. Test với Standalone Sample

### Test library trực tiếp

```bash
cd /home/cvedix/core_ai_runtime

# Cần có:
# 1. TensorRT engine: arcface_r50_fp16.engine
# 2. Test images: test_face1.jpg, test_face2.jpg (112x112 aligned faces)

# Run test
export LD_LIBRARY_PATH=/home/cvedix/core_ai_runtime/build/libs:$LD_LIBRARY_PATH
./build/samples/face_recognition_test \
    ./cvedix_data/models/face/arcface_r50_fp16.engine \
    test_face1.jpg \
    test_face2.jpg
```

**Expected output**:
```
Loading TensorRT engine from: ./cvedix_data/models/face/arcface_r50_fp16.engine
Engine loaded successfully!
Embedding size: 512

Extracting features from 2 image(s)...
Extracted 2 embedding(s)

Image 1 embedding (first 10 values): 0.1234 -0.5678 ... 
Image 2 embedding (first 10 values): 0.2345 -0.3456 ... 

=== Similarity Matrix ===
          Img1    Img2
Img 1   1.0000  0.7823
Img 2   0.7823  1.0000

=== Detailed Comparisons ===
Image 1 vs Image 2:
  Cosine Similarity: 0.7823
  L2 Distance: 0.6594
  -> Same person (high similarity)

Test completed successfully!
```

---

## 6. Troubleshooting Common Issues

### Issue 1: "Cannot find libnvinfer.so"

```bash
# Check TensorRT installation
ls /usr/local/tensorRT/lib/libnvinfer.so*

# If not found, verify TensorRT path
export LD_LIBRARY_PATH=/usr/local/tensorRT/lib:$LD_LIBRARY_PATH

# Or update CMakeLists.txt
set(TRT_LIB_PATH "/your/actual/tensorrt/lib/path")
```

### Issue 2: "Wrong tensor names"

```bash
# Check actual tensor names in ONNX model
trtexec --onnx=model.onnx --verbose 2>&1 | grep "Tensor\|Input\|Output"

# Update in insight_face_recognition.h if needed:
static constexpr const char* kInputTensorName = "actual_name";
static constexpr const char* kOutputTensorName = "actual_name";
```

### Issue 3: "CUDA out of memory"

```bash
# Check GPU memory
nvidia-smi

# Reduce batch size in insight_face_recognition.h:
static constexpr int kBatchSize = 4;  // from 8

# Rebuild
cd build && make clean && make -j8
```

### Issue 4: Compile errors với trt_vehicle/trt_yolov8

**Giải pháp**: Các libraries này đã được temporarily disabled vì incompatibility với TensorRT 10.x. Chỉ `trt_insightface` được build.

Nếu cần `trt_vehicle` hoặc `trt_yolov8`:
- Option A: Downgrade xuống TensorRT 8.6.1
- Option B: Update code để tương thích TensorRT 10+ (complex task)

---

## 7. Verification Checklist

### ✅ Build Success Indicators

1. **Libraries created**:
```bash
ls -lh build/libs/
# Should see:
# - libcvedix_instance_sdk.so  (~38MB)
# - libtrt_insightface.so      (~1.5MB)
```

2. **Samples built**:
```bash
ls -lh build/samples/
# Should see:
# - face_recognition_test
```

3. **Node symbols present**:
```bash
nm -D build/libs/libcvedix_instance_sdk.so | grep insight_face | wc -l
# Should show > 10 symbols
```

### ✅ Runtime Success Indicators

1. **Engine loads successfully**:
```cpp
recognizer->extract_features(faces, embeddings);
// No errors, embeddings populated
```

2. **Embeddings có dimension đúng**:
```cpp
assert(embeddings[0].size() == 512);
```

3. **Similarity values reasonable**:
```cpp
// Same person: > 0.6
// Different person: < 0.4
```

---

## 8. Performance Optimization

### GPU Selection

```cpp
// Trong insight_face_recognition.h, thay đổi:
static constexpr int kGpuId = 0;  // Use GPU 0
```

### Batch Size Tuning

```cpp
// Optimal batch sizes:
// RTX 3080: 8-16
// RTX 2060: 4-8
// GTX 1080: 4

static constexpr int kBatchSize = 8;
```

### Precision Selection

```bash
# FP16 (recommended - 2-3x faster, minimal accuracy loss)
trtexec --onnx=model.onnx --saveEngine=model_fp16.engine --fp16

# FP32 (slower but more compatible)
trtexec --onnx=model.onnx --saveEngine=model_fp32.engine

# INT8 (fastest, requires calibration)
trtexec --onnx=model.onnx --saveEngine=model_int8.engine --int8
```

---

## 9. Quick Reference

### Build Commands Summary

```bash
# Clean build
cd /home/cvedix/core_ai_runtime/build
rm -rf *
cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_TRT=ON ..
make -j8

# Rebuild specific target
make cvedix_instance_sdk -j8
make trt_insightface -j8
make face_recognition_test
```

### Environment Variables

```bash
# Add to ~/.bashrc
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:/usr/local/tensorRT/lib:$LD_LIBRARY_PATH
export PATH=/usr/local/cuda/bin:/usr/local/tensorRT/bin:$PATH
```

### File Locations

```
/home/cvedix/core_ai_runtime/
├── build/
│   ├── libs/
│   │   ├── libcvedix_instance_sdk.so       ← Main library
│   │   └── libtrt_insightface.so           ← InsightFace TRT library
│   └── samples/
│       └── face_recognition_test           ← Test program
├── cvedix_data/
│   └── models/
│       └── face/
│           └── arcface_r50_fp16.engine     ← TensorRT model
└── nodes/infers/
    ├── cvedix_trt_insight_face_recognition_node.h
    └── cvedix_trt_insight_face_recognition_node.cpp
```

---

## 10. Next Steps

1. **Chuẩn bị model**: Download và convert InsightFace ONNX model
2. **Test standalone**: Chạy `face_recognition_test` sample
3. **Tích hợp pipeline**: Tạo pipeline với detector + recognizer
4. **Build database**: Register known faces
5. **Production deploy**: Package và deploy lên server

---

## 11. Known Limitations (Current Version)

- ✅ **trt_insightface**: Full support, TensorRT 10.x compatible
- ⚠️ **trt_vehicle**: Temporarily disabled (TensorRT 8.x code)
- ⚠️ **trt_yolov8**: Temporarily disabled (TensorRT 8.x code)

**Impact**: Chỉ có `cvedix_trt_insight_face_recognition_node` available. Các TensorRT vehicle nodes không compile được.

**Workaround**: Sử dụng OpenCV DNN variants:
- `cvedix_yolo_detector_node` (thay vì trt_yolov8)
- `cvedix_yunet_face_detector_node` (face detection)
- `cvedix_sface_feature_encoder_node` (alternative face recognition)

---

## Tài liệu tham khảo

- [Chi tiết Face Recognition](./FACE_RECOGNITION_INSIGHTFACE.md)
- [Model Preparation Guide](../third_party/trt_insightface/MODEL_PREPARATION.md)
- [Library README](../third_party/trt_insightface/README.md)
- [General Build Guide](./env.md)


