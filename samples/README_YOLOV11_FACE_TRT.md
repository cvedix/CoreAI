# YOLOv11 Face Detection TensorRT Samples

## Tổng quan

TensorRT-accelerated YOLOv11 Face Detection với high-performance inference trên NVIDIA GPU.

### Features
- ✅ TensorRT FP16 inference (~700+ FPS trên RTX 3060 Ti với v11n)
- ✅ 5-point facial landmarks (nếu model hỗ trợ)
- ✅ Real-time visualization với OSD
- ✅ Pipeline-based architecture

---

## Build

### Requirements

- ✅ NVIDIA GPU với CUDA support
- ✅ TensorRT >= 8.x (tested với 10.9)
- ✅ Build với `-DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON`
- ✅ Build với `-DCVEDIX_BUILD_SAMPLES=ON`

### Build Commands

```bash
cd /home/cvedix/Documents/core_ai_runtime
mkdir -p build && cd build

# Configure
cmake -DCVEDIX_WITH_CUDA=ON \
      -DCVEDIX_WITH_TRT=ON \
      -DCVEDIX_BUILD_SAMPLES=ON \
      ..

# Build sample
make yolov11_face_detector_trt_sample -j8
```

### Verify Build

```bash
ls -lh build/bin/yolov11_face_detector_trt_sample
# Output: -rwxrwxr-x 1 user user 800K ... yolov11_face_detector_trt_sample
```

---

## Model Preparation

### Step 1: Convert ONNX to TensorRT Engine

```bash
# Create output directory
mkdir -p cvedix_data/models/trt/face

# Convert to FP16 engine
/usr/local/tensorRT/bin/trtexec \
    --onnx=./cvedix_data/models/face/face_detection_yolov11.onnx \
    --saveEngine=./cvedix_data/models/trt/face/yolov11_face_fp16.engine \
    --fp16 \
    --memPoolSize=workspace:4096

# Or convert FP16 ONNX (smaller file)
/usr/local/tensorRT/bin/trtexec \
    --onnx=./cvedix_data/models/face/face_detection_yolov11_fp16.onnx \
    --saveEngine=./cvedix_data/models/trt/face/yolov11_face_fp16.engine \
    --fp16 \
    --memPoolSize=workspace:4096
```

### Step 2: Verify Engine

```bash
ls -lh cvedix_data/models/trt/face/
# Output: yolov11_face_fp16.engine
```

---

## Usage

### Basic Usage

```bash
cd /home/cvedix/Documents/core_ai_runtime

# Set library path
export LD_LIBRARY_PATH=./build/libs:$LD_LIBRARY_PATH

# Run với defaults
./build/bin/yolov11_face_detector_trt_sample
```

### With Custom Arguments

```bash
./build/bin/yolov11_face_detector_trt_sample \
    ./cvedix_data/models/trt/face/yolov11_face_fp16.engine \
    ./cvedix_data/test_video/face.mp4 \
    0.5 \   # confidence threshold
    0.45    # NMS threshold
```

### Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| engine_path | `./cvedix_data/models/trt/face/yolov11_face_fp16.engine` | TensorRT engine file |
| video_path | `./cvedix_data/test_video/face.mp4` | Input video file |
| conf_threshold | `0.5` | Confidence threshold |
| nms_threshold | `0.45` | NMS IoU threshold |

### Help

```bash
./build/bin/yolov11_face_detector_trt_sample --help
```

---

## Pipeline Architecture

```
┌──────────────┐    ┌───────────────────┐    ┌─────────┐    ┌──────────────┐
│  file_src    │───▶│  face_detector    │───▶│   osd   │───▶│  screen_des  │
│  (Video)     │    │  (TRT YOLOv11)    │    │  (Draw) │    │  (Display)   │
└──────────────┘    └───────────────────┘    └─────────┘    └──────────────┘
```

### Output Data

Detection results are stored in `frame_meta->face_targets` as `cvedix_frame_face_target`:

```cpp
struct cvedix_frame_face_target {
    int x, y, width, height;           // Bounding box
    float score;                       // Confidence
    std::vector<std::pair<int, int>> key_points;  // 5-point landmarks
    std::vector<float> embeddings;     // (filled by recognition node)
    std::string identify;              // (filled by recognition node)
    float identify_score;              // (filled by recognition node)
};
```

---

## Code Example

### Basic Pipeline

```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

// Create nodes
auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
    "file_src", 0, "video.mp4", 1.0f, true);
    
auto face_detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_face_detector_node>(
    "face_detector", "yolov11_face.engine", 0.5f, 0.45f);
    
auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

// Build pipeline
face_detector->attach_to({file_src});
osd->attach_to({face_detector});
screen_des->attach_to({osd});

// Start
file_src->start();
```

### With Custom Hook

```cpp
// Process detected faces
auto face_hook = [](std::string node_name, int queue_size,
                    std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm) return;
    
    for (auto& face : fm->face_targets) {
        std::cout << "Face: (" << face->x << "," << face->y << ") "
                  << face->width << "x" << face->height 
                  << " conf=" << face->score << std::endl;
                  
        // Access landmarks if available
        if (!face->key_points.empty()) {
            std::cout << "  Landmarks: ";
            for (auto& kp : face->key_points) {
                std::cout << "(" << kp.first << "," << kp.second << ") ";
            }
            std::cout << std::endl;
        }
    }
};

face_detector->set_meta_handled_hooker(face_hook);
```

---

## Performance

### Benchmark (RTX 3060 Ti, FP16)

| Model Variant | FPS | Notes |
|--------------|-----|-------|
| YOLOv11n-face | ~700 | Fastest, slight accuracy trade-off |
| YOLOv11s-face | ~500 | Balanced |
| YOLOv11m-face | ~300 | Most accurate |

### Memory Usage

- Engine loading: ~200-500MB VRAM
- Per-frame inference: ~50-100MB (depends on batch size)

---

## Multi-Face Tracking với ByteTrack

Kết hợp YOLOv11 TRT với ByteTrack để tracking nhiều khuôn mặt với persistent IDs.

### Build Sample

```bash
make yolov11_face_bytetrack_sample -j8
```

### Usage

```bash
./build/bin/yolov11_face_bytetrack_sample \
    ./models/yolov11_face_fp16.engine \
    ./video/input.mp4 \
    [output_dir] [conf] [nms]
```

### Pipeline Architecture

```
┌──────────┐    ┌──────────────┐    ┌───────────┐    ┌───────┐    ┌────────┐
│ file_src │───▶│ yolov11_face │───▶│ bytetrack │───▶│  osd  │───▶│ screen │
│ (Video)  │    │ (TRT Detect) │    │ (Tracker) │    │(Draw) │    │        │
└──────────┘    └──────────────┘    └───────────┘    └───────┘    └────────┘
```

### ByteTrack Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `track_thresh` | 0.5 | Minimum confidence to be tracked |
| `high_thresh` | 0.9 | High confidence for first association |
| `match_thresh` | 0.8 | IOU threshold for matching |
| `track_buffer` | 30 | Frames to keep lost tracks |
| `frame_rate` | 30 | Expected FPS |

### Code Example

```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_trt_yolov11_face_detector_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

// 1. Source
auto src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
    "src", 0, "video.mp4", 1.0f, true);

// 2. Face Detector (TensorRT)
auto detector = std::make_shared<cvedix_nodes::cvedix_trt_yolov11_face_detector_node>(
    "detector", "yolov11_face.engine", 0.5f, 0.45f);

// 3. ByteTrack Tracker
auto tracker = std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
    "tracker",
    cvedix_nodes::cvedix_track_for::FACE,  // Track face targets
    0.5f,   // track_thresh
    0.9f,   // high_thresh  
    0.8f,   // match_thresh
    30,     // track_buffer (frames)
    30      // frame_rate
);

// 4. OSD + Screen
auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd");
auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

// Build pipeline
detector->attach_to({src});
tracker->attach_to({detector});
osd->attach_to({tracker});
screen->attach_to({osd});

src->start();
```

### Accessing Track IDs

```cpp
auto stats_hook = [](std::string node_name, int queue_size,
                     std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto fm = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!fm) return;
    
    for (const auto& face : fm->face_targets) {
        std::cout << "Face ID: " << face->track_id 
                  << " at (" << face->x << "," << face->y << ")"
                  << " conf=" << face->score << std::endl;
    }
};
tracker->set_meta_handled_hooker(stats_hook);
```

---

## Integration with Face Recognition

Chain with InsightFace/ArcFace recognition:

```cpp
auto face_detector = std::make_shared<cvedix_trt_yolov11_face_detector_node>(...);
auto face_recognizer = std::make_shared<cvedix_trt_insight_face_recognition_node>(...);

// Pipeline: detector -> recognizer
face_recognizer->attach_to({face_detector});
```

---

## Troubleshooting

### Engine không load được

```bash
# Check engine exists
ls -la ./cvedix_data/models/trt/face/yolov11_face_fp16.engine

# Check TensorRT version compatibility
# Engine phải được build trên cùng TensorRT version với runtime
```

### No faces detected

```bash
# Lower confidence threshold
./build/bin/yolov11_face_detector_trt_sample \
    ./cvedix_data/models/trt/face/yolov11_face_fp16.engine \
    ./cvedix_data/test_video/face.mp4 \
    0.3  # Lower threshold
```

### Library not found

```bash
export LD_LIBRARY_PATH=./build/libs:$LD_LIBRARY_PATH
```

---

## Related Documentation

- [TensorRT Samples](README_TENSORRT_SAMPLES.md)
- [InsightFace TRT](README_INSIGHTFACE_TRT.md)
- [Face Samples](README_FACE_SAMPLES.md)
- [Model Preparation](../third_party/trt_yolov11_face/README.md)

---

**Happy coding! 🚀**
