# Detection & Segmentation Samples

## Tổng quan

Các samples này demo các chức năng detection và segmentation: object detection, pose estimation, semantic segmentation, và các detection chuyên biệt.

---

## Sample 1: OpenPose Pose Estimation

### File
`openpose_sample.cpp`

### Mô tả
Pose estimation sử dụng OpenPose network để detect keypoints của cơ thể.

### Pipeline
```
Video → OpenPose Detector → Pose OSD → Screen
```

### Features
- Body pose estimation
- 25 keypoints detection (body_25)
- Real-time visualization

### Usage
```bash
./build/bin/openpose_sample
```

### Configuration
```cpp
auto openpose = std::make_shared<cvedix_openpose_detector_node>(
    "openpose",
    "./cvedix_data/models/openpose/pose/body_25_pose_iter_584000.caffemodel",
    "./cvedix_data/models/openpose/pose/body_25_pose_deploy.prototxt",
    "",                    // weights path
    368,                   // input width
    368,                   // input height
    1,                     // batch size
    0,                     // num_classes
    0.1,                   // threshold
    cvedix_objects::cvedix_pose_type::body_25
);
```

### Pose Types
- `body_25`: 25 keypoints (recommended)
- `coco_18`: 18 keypoints (COCO format)
- `mpi_15`: 15 keypoints (MPI format)

### Model Requirements
- Caffe model files (.caffemodel, .prototxt)
- Download từ OpenPose repository

---

## Sample 2: Mask R-CNN Segmentation

### File
`mask_rcnn_sample.cpp`

### Mô tả
Image segmentation sử dụng Mask R-CNN để detect objects và masks.

### Pipeline
```
Video → Mask R-CNN → Tracker → OSD → Screen
```

### Features
- Object detection
- Instance segmentation (masks)
- Multi-class support (COCO 80 classes)
- Tracking integration

### Usage
```bash
./build/bin/mask_rcnn_sample
```

### Configuration
```cpp
auto mask_rcnn = std::make_shared<cvedix_mask_rcnn_detector_node>(
    "mask_rcnn",
    "./cvedix_data/models/mask_rcnn/frozen_inference_graph.pb",
    "./cvedix_data/models/mask_rcnn/mask_rcnn_inception_v2_coco_2018_01_28.pbtxt",
    "./cvedix_data/models/coco_80classes.txt"
);
```

### Output
- Bounding boxes
- Segmentation masks
- Class labels
- Confidence scores

---

## Sample 3: ENet Semantic Segmentation

### File
`enet_seg_sample.cpp`

### Mô tả
Semantic segmentation sử dụng ENet network.

### Pipeline
```
Video → ENet → Segmentation OSD → Screen
```

### Features
- Pixel-level classification
- Real-time segmentation
- Multiple classes

### Usage
```bash
./build/bin/enet_seg_sample
```

### Configuration
```cpp
auto enet = std::make_shared<cvedix_enet_seg_node>(
    "enet",
    "./cvedix_data/models/enet/enet-model.net",
    "./cvedix_data/models/enet/enet-classes.txt"
);
```

---

## Sample 4: Lane Detection

### File
`lane_detect_sample.cpp`

### Mô tả
Phát hiện làn đường trên đường phố.

### Pipeline
```
Video → Lane Detector → Lane OSD → [Screen OSD, Screen Original]
```

### Features
- Lane detection
- Multiple lane support
- Real-time visualization
- Dual output (OSD + original)

### Usage
```bash
./build/bin/lane_detect_sample
```

### Configuration
```cpp
auto lane_detector = std::make_shared<cvedix_lane_detector_node>(
    "lane_detector",
    "./cvedix_data/models/lane/lane_det.onnx"
);
```

### Output
- Detected lane lines
- Lane boundaries
- Lane center (if applicable)

---

## Sample 5: Fire & Smoke Detection

### File
`firesmoke_detect_sample.cpp`

### Mô tả
Phát hiện lửa và khói sử dụng YOLO detector.

### Pipeline
```
[Video1, Video2] → YOLO Detector → OSD → Split → [Screen1, Screen2]
```

### Features
- Fire detection
- Smoke detection
- Multi-camera support
- Real-time alerts

### Usage
```bash
./build/bin/firesmoke_detect_sample
```

### Configuration
```cpp
auto detector = std::make_shared<cvedix_yolo_detector_node>(
    "firesmoke_detector",
    "./cvedix_data/models/det_cls/firesmoke_yolov5s.onnx",
    "",
    "./cvedix_data/models/det_cls/firesmoke_3classes.txt",
    640,  // input width
    384   // input height
);
```

### Classes
- Fire
- Smoke
- Background

---

## Sample 6: Obstacle Detection

### File
`obstacle_detect_sample.cpp`

### Mô tả
Phát hiện vật cản trên đường sử dụng YOLO.

### Pipeline
```
[Video1, Video2] → YOLO Detector → OSD → Split → [Screen1, Screen2]
```

### Features
- Road obstacle detection
- Multi-camera support
- Real-time detection

### Usage
```bash
./build/bin/obstacle_detect_sample
```

### Configuration
```cpp
auto detector = std::make_shared<cvedix_yolo_detector_node>(
    "obstacle_detector",
    "./cvedix_data/models/det_cls/obstacles_yolov5s.onnx",
    "",
    "./cvedix_data/models/det_cls/obstacles_2classes.txt",
    640,  // input width
    640   // input height
);
```

### Classes
- Obstacle
- Background

---

## So sánh các Detection Samples

| Sample | Type | Model | Output |
|--------|------|-------|--------|
| **openpose** | Pose estimation | Caffe | Keypoints |
| **mask_rcnn** | Instance segmentation | TensorFlow | Masks + boxes |
| **enet_seg** | Semantic segmentation | ENet | Pixel labels |
| **lane_detect** | Lane detection | ONNX | Lane lines |
| **firesmoke** | Object detection | YOLO | Fire/smoke boxes |
| **obstacle** | Object detection | YOLO | Obstacle boxes |

---

## Build

```bash
cd build
cmake ..
make openpose_sample
make mask_rcnn_sample
make enet_seg_sample
make lane_detect_sample
make firesmoke_detect_sample
make obstacle_detect_sample
```

---

## Model Requirements

### OpenPose
- `body_25_pose_iter_584000.caffemodel`
- `body_25_pose_deploy.prototxt`
- Download từ: https://github.com/CMU-Perceptual-Computing-Lab/openpose

### Mask R-CNN
- `frozen_inference_graph.pb`
- `mask_rcnn_inception_v2_coco_2018_01_28.pbtxt`
- `coco_80classes.txt`

### ENet
- `enet-model.net`
- `enet-classes.txt`

### Lane Detection
- `lane_det.onnx`

### Fire/Smoke & Obstacle
- `firesmoke_yolov5s.onnx`
- `obstacles_yolov5s.onnx`
- Class labels files

---

## Use Cases

### 1. Human Pose Analysis
```bash
# Analyze human poses in video
./build/bin/openpose_sample
```

### 2. Object Segmentation
```bash
# Segment objects with masks
./build/bin/mask_rcnn_sample
```

### 3. Road Analysis
```bash
# Detect lanes
./build/bin/lane_detect_sample

# Detect obstacles
./build/bin/obstacle_detect_sample
```

### 4. Safety Monitoring
```bash
# Detect fire and smoke
./build/bin/firesmoke_detect_sample
```

---

## Performance

### OpenPose
- **CPU**: ~100-200ms per frame
- **GPU**: ~20-40ms per frame

### Mask R-CNN
- **CPU**: ~500-1000ms per frame
- **GPU**: ~50-100ms per frame

### ENet
- **CPU**: ~50-100ms per frame
- **GPU**: ~10-20ms per frame

### YOLO-based
- **CPU**: ~30-60ms per frame
- **GPU**: ~5-15ms per frame

---

## Customization

### Adjust detection threshold
```cpp
auto detector = std::make_shared<cvedix_yolo_detector_node>(
    "detector",
    model_path,
    config_path,
    labels_path,
    input_width,
    input_height,
    score_threshold,  // Default: 0.5
    nms_threshold     // Default: 0.4
);
```

### Filter specific classes
```cpp
// Only detect specific classes
std::vector<int> target_classes = {0, 1};  // Fire, Smoke
// (Implementation depends on node API)
```

---

## Troubleshooting

### Issue: Low detection accuracy
- Adjust detection threshold
- Use higher resolution input
- Check model quality

### Issue: Slow performance
- Use GPU acceleration
- Reduce input resolution
- Use lighter model variant

### Issue: Missing detections
- Lower detection threshold
- Improve video quality
- Check model compatibility

---

## Related Documentation

- [Main README](README.md) - Tổng quan samples
- [Vehicle Samples](README_VEHICLE_SAMPLES.md) - Vehicle detection


