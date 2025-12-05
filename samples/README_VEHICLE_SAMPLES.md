# Vehicle Processing Samples

## Tổng quan

Các samples này demo các chức năng xử lý phương tiện: detection, tracking, body scan, và clustering.

---

## Sample 1: Vehicle Tracking

### File
`vehicle_tracking_sample.cpp`

### Mô tả
Tracking nhiều phương tiện trong video sử dụng SORT algorithm.

### Pipeline
```
Video → Vehicle Detector → SORT Tracker → OSD → Screen
```

### Features
- Vehicle detection với TensorRT
- Multi-vehicle tracking
- Track ID persistence

### Usage
```bash
./build/bin/vehicle_tracking_sample
```

### Requirements
- TensorRT vehicle detector model
- Build với `-DCVEDIX_WITH_TRT=ON`

### Configuration
```cpp
auto detector = std::make_shared<cvedix_trt_vehicle_detector>(
    "detector",
    "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt"
);

auto tracker = std::make_shared<cvedix_sort_track_node>("tracker");
```

---

## Sample 2: Vehicle Body Scan

### File
`vehicle_body_scan_sample.cpp`

### Mô tả
Phát hiện các bộ phận của phương tiện dựa trên góc nhìn bên (side view).

### Pipeline
```
Video → Vehicle Detector → Body Part Detector → OSD → Screen
```

### Features
- Vehicle detection
- Body part detection (doors, windows, etc.)
- Side view analysis

### Usage
```bash
./build/bin/vehicle_body_scan_sample
```

### Use Case
- Vehicle inspection
- Damage detection
- Part analysis

---

## Sample 3: Vehicle Cluster

### File
`vehicle_cluster_based_on_classify_encoding_sample.cpp`

### Mô tả
Clustering phương tiện dựa trên classification labels và feature encoding, hiển thị 3 windows:
1. Cluster by t-SNE visualization
2. Cluster by labels
3. Detection results

### Pipeline
```
Video → Detector → Classifier → Encoder → Cluster → Visualization
```

### Features
- Vehicle detection
- Classification (car, truck, bus, etc.)
- Feature encoding
- t-SNE clustering visualization
- Label-based clustering

### Usage
```bash
./build/bin/vehicle_cluster_based_on_classify_encoding_sample
```

### Output
- 3 visualization windows
- Real-time clustering updates
- Similarity analysis

---

## Sample 4: Body Scan và Plate Detect

### File
`body_scan_and_plate_detect_sample.cpp`

### Mô tả
2 channels song song:
1. Detect vehicle body parts
2. Detect vehicle plates

Có thể thực hiện data fusion sau đó.

### Pipeline
```
Video → Split → [Body Scan, Plate Detect] → [OSD1, OSD2] → [Screen1, Screen2]
```

### Features
- Parallel processing
- Body part detection
- License plate detection
- Multi-channel output

### Usage
```bash
./build/bin/body_scan_and_plate_detect_sample
```

### Use Case
- Comprehensive vehicle analysis
- Multi-task processing
- Data fusion preparation

---

## So sánh các Vehicle Samples

| Sample | Chức năng | Output | Use Case |
|--------|-----------|--------|----------|
| **vehicle_tracking** | Detection + Tracking | Tracked vehicles | Traffic monitoring |
| **vehicle_body_scan** | Body part detection | Parts detected | Inspection |
| **vehicle_cluster** | Clustering + Visualization | 3 windows | Analysis |
| **body_scan_and_plate** | Multi-task | Parallel results | Comprehensive |

---

## Build

```bash
cd build
cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON ..
make vehicle_tracking_sample
make vehicle_body_scan_sample
make vehicle_cluster_based_on_classify_encoding_sample
make body_scan_and_plate_detect_sample
```

---

## Model Requirements

### Vehicle Detection
- `vehicle_v8.5.trt` (TensorRT engine)
- Convert từ ONNX hoặc weights

### Body Part Detection
- Custom TensorRT model
- Trained on side view images

### Classification & Encoding
- Vehicle classifier model
- Feature encoder model

---

## Customization

### Adjust tracking parameters
```cpp
auto tracker = std::make_shared<cvedix_sort_track_node>(
    "tracker",
    cvedix_track_for::NORMAL,
    max_age,      // Default: 30
    min_hits,     // Default: 3
    iou_threshold // Default: 0.3
);
```

### Change detection threshold
```cpp
auto detector = std::make_shared<cvedix_trt_vehicle_detector>(
    "detector",
    model_path,
    score_threshold,  // Default: 0.5
    nms_threshold     // Default: 0.4
);
```

---

## Use Cases

### 1. Traffic Monitoring
```bash
# Track vehicles in traffic
./build/bin/vehicle_tracking_sample
```

### 2. Vehicle Inspection
```bash
# Scan vehicle body parts
./build/bin/vehicle_body_scan_sample
```

### 3. Vehicle Analysis
```bash
# Cluster and analyze vehicles
./build/bin/vehicle_cluster_based_on_classify_encoding_sample
```

### 4. Comprehensive Analysis
```bash
# Multi-task vehicle processing
./build/bin/body_scan_and_plate_detect_sample
```

---

## Performance

### Tracking Performance
- **Detection**: ~10-15ms per frame
- **Tracking**: ~1-2ms per frame
- **Total**: ~12-17ms per frame

### Clustering Performance
- **Inference**: ~15-20ms per frame
- **Clustering**: ~5-10ms per batch
- **Visualization**: ~2-5ms per update

---

## Troubleshooting

### Issue: Low detection accuracy
- Adjust detection threshold
- Use higher resolution input
- Check model quality

### Issue: Tracking lost
- Increase max_age parameter
- Improve detection quality
- Use higher frame rate

### Issue: Slow clustering
- Reduce batch size
- Use GPU acceleration
- Optimize visualization update rate

---

## Related Documentation

- [Behavior Analysis Samples](README_BEHAVIOR_ANALYSIS_SAMPLES.md) - Vehicle behavior
- [Main README](README.md) - Tổng quan samples

