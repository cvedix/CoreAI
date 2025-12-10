# FaceNet Quick Start Guide

Hướng dẫn nhanh để sử dụng FaceNet trong 5 phút.

## Bước 1: Cài đặt Python dependencies

```bash
pip install torch torchvision facenet-pytorch onnx onnx-simplifier
```

## Bước 2: Export model sang ONNX

```bash
cd /home/cvedix/core_ai_runtime/scripts
python export_facenet_to_onnx.py \
    --dataset vggface2 \
    --output ../cvedix_data/models/face/facenet_vggface2.onnx
```

Output:
```
======================================================================
Exporting FaceNet to ONNX
======================================================================
✓ Model loaded successfully (pretrained on vggface2)
✓ Forward pass successful, output shape: torch.Size([1, 512])
✓ Model exported to ../cvedix_data/models/face/facenet_vggface2.onnx
✓ ONNX model is valid
✓ OpenCV DNN inference successful
======================================================================
```

## Bước 3: Build cvedix với FaceNet

```bash
cd /home/cvedix/core_ai_runtime
mkdir -p build && cd build
cmake -DCVEDIX_WITH_CUDA=ON ..
make -j$(nproc)
```

## Bước 4: Tạo face database

Tạo file `face_database.txt` với format:

```
person_name embedding[0] embedding[1] ... embedding[511]
```

Ví dụ với 2 người:

```bash
cd /home/cvedix/core_ai_runtime/build

# Tạo thư mục chứa ảnh mẫu
mkdir -p test_faces/john_doe
mkdir -p test_faces/jane_smith

# Copy ảnh vào (thay bằng ảnh thực tế của bạn)
cp /path/to/john_photo.jpg test_faces/john_doe/
cp /path/to/jane_photo.jpg test_faces/jane_smith/

# Chạy registration (chức năng này cần implement)
# ./bin/facenet_register_sample test_faces/ face_database.txt
```

## Bước 5: Chạy face recognition

### Option A: Từ video file

```bash
./bin/facenet_sample video.mp4 face_database.txt
```

### Option B: Từ webcam

```bash
./bin/facenet_sample 0 face_database.txt
```

### Option C: Từ RTSP stream

```bash
./bin/facenet_sample rtsp://camera_ip:554/stream face_database.txt
```

## Code Example - Minimal

```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_facenet_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

int main() {
    // Source
    auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "src", "video.mp4");
    
    // Detector
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
        "detector", "models/yunet.onnx", 640, 640);
    
    // FaceNet
    auto facenet = std::make_shared<cvedix_nodes::cvedix_facenet_node>(
        "facenet", "models/facenet_vggface2.onnx");
    
    // Display
    auto display = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("display");
    
    // Pipeline
    src->set_next({detector});
    detector->set_next({facenet});
    facenet->set_next({display});
    
    // Run
    display->start();
    facenet->start();
    detector->start();
    src->start();
    src->wait();
    
    return 0;
}
```

## Expected Output

```
[INFO] === FaceNet Face Recognition Sample ===
[INFO] Video: video.mp4
[INFO] Database: face_database.txt
[INFO] Model: models/facenet_vggface2.onnx
[INFO] Loaded 2 faces from database
[INFO] Loaded FaceNet ONNX model: models/facenet_vggface2.onnx
  - Pretrained on: vggface2
  - Input size: 160x160
  - Embedding size: 512
  - Face alignment: enabled
[INFO] Pipeline started successfully!
[INFO] Detected 1 faces (threshold=0.60)
[DEBUG] Face matched: john_doe (score=0.876)
...
[INFO] === Statistics ===
[INFO] Total faces processed: 147
[INFO] Sample completed successfully
```

## Verification

Kiểm tra kết quả:

```cpp
// In hook after facenet node
facenet->add_hook([](auto meta) {
    for (auto& face : meta->face_targets) {
        // Check embedding
        assert(face->embeddings.size() == 512);
        
        // Check L2 norm (should be ~1.0)
        float norm = 0.0f;
        for (float v : face->embeddings) {
            norm += v * v;
        }
        norm = std::sqrt(norm);
        std::cout << "Embedding norm: " << norm << std::endl;
        assert(std::abs(norm - 1.0f) < 0.01f);
        
        // Check matching result
        std::cout << "Matched: " << face->primary_class_name 
                  << " (score=" << face->confidence << ")" << std::endl;
    }
});
```

## Common Issues

### 1. "Failed to load ONNX model"

**Check:**
```bash
# Verify file exists
ls -lh cvedix_data/models/face/facenet_vggface2.onnx

# Verify ONNX model
python -c "import onnx; onnx.checker.check_model('cvedix_data/models/face/facenet_vggface2.onnx')"
```

### 2. "No face detected"

**Solutions:**
- Lower detection threshold: `detector->score_threshold = 0.5f;`
- Check input image quality
- Ensure face is visible and not too small

### 3. Low matching accuracy

**Solutions:**
- Enable face alignment: `enable_alignment = true`
- Use YuNet detector (provides better landmarks)
- Register multiple photos per person
- Check lighting conditions

### 4. Slow performance

**Solutions:**
- Build with CUDA: `-DCVEDIX_WITH_CUDA=ON`
- Reduce input resolution
- Use YuNet instead of MTCNN
- Disable face alignment if not needed

## Next Steps

1. **Detailed Guide:** Read `FACENET_GUIDE.md` for complete documentation
2. **Sample Code:** Check `samples/facenet_sample.cpp` for full example
3. **API Reference:** See `nodes/infers/cvedix_facenet_node.h` for class documentation
4. **Performance:** Read performance optimization section in guide

## Support

- Documentation: `/home/cvedix/core_ai_runtime/doc/FACENET_GUIDE.md`
- Sample code: `/home/cvedix/core_ai_runtime/samples/facenet_sample.cpp`
- Issues: Contact support@cvedix.com




