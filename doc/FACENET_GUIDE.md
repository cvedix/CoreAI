# FaceNet Face Recognition Guide

## Tổng quan

`cvedix_facenet_node` là node nhận dạng khuôn mặt dựa trên kiến trúc FaceNet (InceptionResnetV1) được chuyển đổi từ [facenet-pytorch](https://github.com/timesler/facenet-pytorch) sang C/C++ sử dụng ONNX và OpenCV DNN.

### Tính năng chính

- ✅ Kiến trúc InceptionResnetV1 cho face recognition
- ✅ Pretrained trên VGGFace2 (8.6M faces) hoặc CASIA-Webface (0.5M faces)
- ✅ Embedding 512 chiều, L2 normalized
- ✅ Face alignment sử dụng 5-point landmarks (optional)
- ✅ Hỗ trợ CUDA và CPU backend
- ✅ Tương thích với OpenCV DNN

### Kiến trúc

```
┌─────────────────┐      ┌──────────────────┐      ┌─────────────────┐
│  Face Detector  │ ───> │  FaceNet Node    │ ───> │  Face Database  │
│  (YuNet/MTCNN)  │      │  (Recognition)   │      │  (Matching)     │
└─────────────────┘      └──────────────────┘      └─────────────────┘
     │                           │                           │
     │ Detects faces            │ Extracts 512-d           │ Finds matches
     │ with landmarks           │ embeddings               │ using cosine
     │                           │                           │ similarity
```

## Cài đặt

### 1. Yêu cầu

- OpenCV >= 4.6 (with DNN module)
- CMake >= 3.10
- C++20 compiler
- Python 3.7+ (để export models)
- CUDA (optional, cho GPU acceleration)

### 2. Cài đặt Python dependencies

```bash
pip install torch torchvision facenet-pytorch onnx onnx-simplifier
```

### 3. Export FaceNet models sang ONNX

```bash
cd /home/cvedix/core_ai_runtime/scripts

# Export VGGFace2 model (recommended)
python export_facenet_to_onnx.py \
    --dataset vggface2 \
    --output ../cvedix_data/models/face/facenet_vggface2.onnx

# Export CASIA-Webface model
python export_facenet_to_onnx.py \
    --dataset casia-webface \
    --output ../cvedix_data/models/face/facenet_casia.onnx

# Export MTCNN detector (optional)
python export_facenet_to_onnx.py \
    --mtcnn \
    --output-dir ../cvedix_data/models/face/
```

### 4. Build cvedix với FaceNet node

```bash
cd /home/cvedix/core_ai_runtime
mkdir -p build && cd build

# Build with CPU
cmake -DCVEDIX_WITH_CUDA=OFF ..
make -j$(nproc)

# Build with CUDA (recommended for better performance)
cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_TRT=OFF ..
make -j$(nproc)
```

## Sử dụng

### 1. Basic Face Recognition Pipeline

```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_facenet_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

int main() {
    // 1. Video source
    auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "src", "video.mp4", 0, 30, 640, 480, false
    );
    
    // 2. Face detector (YuNet recommended)
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
        "face_detector",
        "cvedix_data/models/face/face_detection_yunet_2023mar.onnx",
        640, 640,  // input size
        0.6f,      // score threshold
        0.3f       // nms threshold
    );
    
    // 3. FaceNet recognition
    auto facenet = std::make_shared<cvedix_nodes::cvedix_facenet_node>(
        "facenet",
        "cvedix_data/models/face/facenet_vggface2.onnx",
        160, 160,      // input size (standard FaceNet)
        true,          // enable alignment
        "vggface2"     // pretrained dataset
    );
    
    // 4. OSD for visualization
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd");
    
    // 5. Display output
    auto display = std::make_shared<cvedix_nodes::cvedix_screen_des_node>(
        "display", "Face Recognition"
    );
    
    // Build pipeline
    src->set_next({detector});
    detector->set_next({facenet});
    facenet->set_next({osd});
    osd->set_next({display});
    
    // Start pipeline
    src->start();
    detector->start();
    facenet->start();
    osd->start();
    display->start();
    
    // Wait for completion
    src->wait();
    
    return 0;
}
```

### 2. Face Registration (Đăng ký khuôn mặt)

```cpp
#include <fstream>
#include <opencv2/opencv.hpp>

// Function to register a face
void register_face(
    const std::string& person_name,
    const std::string& image_path,
    const std::string& database_path
) {
    // Load image
    cv::Mat image = cv::imread(image_path);
    
    // Detect face
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
        "detector", "models/yunet.onnx", 640, 640
    );
    
    // Extract embedding
    auto facenet = std::make_shared<cvedix_nodes::cvedix_facenet_node>(
        "facenet", "models/facenet_vggface2.onnx"
    );
    
    // Create frame meta
    auto frame_meta = std::make_shared<cvedix_objects::cvedix_frame_meta>();
    frame_meta->frame = image;
    
    // Detect and recognize
    detector->process(frame_meta);
    if (frame_meta->face_targets.empty()) {
        std::cerr << "No face detected!" << std::endl;
        return;
    }
    
    facenet->process(frame_meta);
    
    // Get embedding
    auto& embedding = frame_meta->face_targets[0]->embeddings;
    
    // Save to database
    std::ofstream db(database_path, std::ios::app);
    db << person_name;
    for (float val : embedding) {
        db << " " << val;
    }
    db << std::endl;
    db.close();
    
    std::cout << "Registered: " << person_name << std::endl;
}
```

### 3. Face Matching (So khớp khuôn mặt)

```cpp
#include <cmath>
#include <map>

// Face database
struct FaceDatabase {
    std::map<std::string, std::vector<float>> embeddings;
    
    void load(const std::string& path) {
        std::ifstream db(path);
        std::string name;
        while (db >> name) {
            std::vector<float> emb(512);
            for (int i = 0; i < 512; i++) {
                db >> emb[i];
            }
            embeddings[name] = emb;
        }
    }
    
    // Compute cosine similarity
    float cosine_similarity(
        const std::vector<float>& a,
        const std::vector<float>& b
    ) {
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }
    
    // Find best match
    std::pair<std::string, float> find_match(
        const std::vector<float>& query_embedding,
        float threshold = 0.6f
    ) {
        std::string best_name = "Unknown";
        float best_score = -1.0f;
        
        for (const auto& [name, emb] : embeddings) {
            float score = cosine_similarity(query_embedding, emb);
            if (score > best_score && score > threshold) {
                best_score = score;
                best_name = name;
            }
        }
        
        return {best_name, best_score};
    }
};

// Usage in pipeline
void setup_recognition_with_matching(FaceDatabase& db) {
    // ... setup detector and facenet ...
    
    // Add hook to perform matching
    facenet->add_hook([&db](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        for (auto& face : meta->face_targets) {
            if (!face->embeddings.empty()) {
                auto [name, score] = db.find_match(face->embeddings);
                face->primary_class_name = name;
                face->confidence = score;
            }
        }
    });
}
```

## So sánh với InsightFace

| Feature | FaceNet | InsightFace |
|---------|---------|-------------|
| Architecture | InceptionResnetV1 | ResNet/MobileFaceNet |
| Embedding size | 512 | 512 |
| Pretrained datasets | VGGFace2, CASIA-Webface | MS1MV2, Glint360K |
| Input size | 160x160 | 112x112 |
| Normalization | (x/255 - 0.5)/0.5 | (x - 127.5)/128 |
| Performance | Good | Better |
| Speed | Slower | Faster |
| Accuracy | ~99.2% (VGGFace2) | ~99.8% (MS1MV2) |

**Khuyến nghị:**
- Sử dụng **FaceNet** nếu cần tương thích với facenet-pytorch ecosystem
- Sử dụng **InsightFace** (`cvedix_insight_face_recognition_node`) nếu cần performance tốt hơn

## Face Detectors tương thích

### 1. YuNet (Recommended)

```cpp
auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
    "detector",
    "models/face_detection_yunet_2023mar.onnx",
    640, 640,
    0.6f,  // score threshold
    0.3f   // nms threshold
);
```

**Ưu điểm:**
- Nhanh và chính xác
- 5-point landmarks cho face alignment
- Hỗ trợ multi-scale detection

### 2. MTCNN (Best for FaceNet)

```cpp
auto detector = std::make_shared<cvedix_nodes::cvedix_mtcnn_face_detector_node>(
    "mtcnn",
    "models/mtcnn_pnet.onnx",
    "models/mtcnn_rnet.onnx",
    "models/mtcnn_onet.onnx",
    20.0f,  // min face size
    {0.6f, 0.7f, 0.7f}  // thresholds [P, R, O]
);
```

**Ưu điểm:**
- Thiết kế để dùng với FaceNet
- Chất lượng landmarks tốt
- Robust với pose variations

**Nhược điểm:**
- Chậm hơn YuNet (3 stages)

### 3. RKNN Face Detector (Rockchip NPU)

```cpp
#ifdef CVEDIX_WITH_RKNN
auto detector = std::make_shared<cvedix_nodes::cvedix_rknn_face_detector_node>(
    "detector",
    "models/yolov8n-face.rknn",
    640, 640
);
#endif
```

## Performance Optimization

### 1. CUDA Acceleration

```bash
# Build with CUDA
cmake -DCVEDIX_WITH_CUDA=ON ..
make -j$(nproc)
```

Performance với CUDA:
- CPU (Intel i7): ~15 FPS
- CUDA (GTX 1080): ~60 FPS

### 2. Batch Processing

```cpp
// Enable batch processing (không khả dụng cho secondary infer node)
// FaceNet xử lý từng face một, nhưng có thể optimize bằng cách
// batch nhiều faces trong một frame
```

### 3. Model Optimization

```bash
# Simplify ONNX model
pip install onnx-simplifier
python -c "
import onnx
import onnxsim
model = onnx.load('facenet.onnx')
model_simp, check = onnxsim.simplify(model)
onnx.save(model_simp, 'facenet_simplified.onnx')
"
```

## Troubleshooting

### 1. Model không load được

**Lỗi:** `Failed to load FaceNet ONNX model`

**Giải pháp:**
- Kiểm tra đường dẫn model
- Verify ONNX model: `python -c "import onnx; onnx.checker.check_model('model.onnx')"`
- Re-export model với opset version 11

### 2. Embedding không đúng

**Triệu chứng:** Cosine similarity luôn thấp

**Giải pháp:**
- Kiểm tra normalization: embeddings phải được L2 normalized
- Verify preprocessing: `(pixel/255 - 0.5) / 0.5`
- So sánh output với PyTorch model gốc

### 3. Face alignment không hoạt động

**Triệu chứng:** Accuracy thấp dù có landmarks

**Giải pháp:**
- Kiểm tra landmarks format: phải có 5 điểm [left_eye, right_eye, nose, left_mouth, right_mouth]
- Sử dụng face detector hỗ trợ landmarks (YuNet, MTCNN)
- Disable alignment nếu landmarks không chính xác: `enable_alignment = false`

### 4. Performance kém

**Giải pháp:**
- Enable CUDA: `-DCVEDIX_WITH_CUDA=ON`
- Giảm input size detector (trade-off accuracy)
- Sử dụng YuNet thay vì MTCNN
- Skip face alignment nếu không cần thiết

## Testing

### 1. Unit Test

```bash
cd build
./test/facenet_test
```

### 2. Benchmark

```bash
# Benchmark inference speed
./samples/benchmark_facenet \
    --model cvedix_data/models/face/facenet_vggface2.onnx \
    --images test_images/ \
    --repeat 100
```

### 3. Accuracy Test

```bash
# Test on LFW dataset
./samples/test_facenet_lfw \
    --model cvedix_data/models/face/facenet_vggface2.onnx \
    --lfw-dir /path/to/lfw \
    --pairs-file pairs.txt
```

Expected accuracy:
- VGGFace2 model: ~99.2% on LFW
- CASIA-Webface model: ~98.8% on LFW

## References

1. **FaceNet paper:**  
   Schroff, F., Kalenichenko, D., & Philbin, J. (2015). FaceNet: A unified embedding for face recognition and clustering. CVPR 2015.  
   https://arxiv.org/abs/1503.03832

2. **facenet-pytorch:**  
   https://github.com/timesler/facenet-pytorch

3. **VGGFace2 dataset:**  
   Cao, Q., Shen, L., Xie, W., Parkhi, O. M., & Zisserman, A. (2018). VGGFace2: A dataset for recognising faces across pose and age. FG 2018.

4. **MTCNN paper:**  
   Zhang, K., Zhang, Z., Li, Z., & Qiao, Y. (2016). Joint face detection and alignment using multitask cascaded convolutional networks. IEEE Signal Processing Letters.

## License

Mã nguồn `cvedix_facenet_node` tuân theo license của cvedix_ai_runtime.

Pretrained models từ facenet-pytorch tuân theo MIT License.

## Support

Liên hệ: support@cvedix.com




