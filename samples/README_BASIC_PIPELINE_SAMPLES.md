# Basic Pipeline Samples

## Tổng quan

Các samples này demo các kiến trúc pipeline cơ bản trong framework, giúp hiểu cách kết nối các nodes với nhau.

---

## Sample 1: 1-1-1 Pipeline

### File
`1-1-1_sample.cpp`

### Mô tả
Pipeline đơn giản nhất: **1 input → 1 inference → 1 output**

### Pipeline Structure
```
Video File → Face Detector → Face Encoder → OSD → Screen
```

### Code Structure
```cpp
auto file_src = std::make_shared<cvedix_file_src_node>(...);
auto detector = std::make_shared<cvedix_yunet_face_detector_node>(...);
auto encoder = std::make_shared<cvedix_sface_feature_encoder_node>(...);
auto osd = std::make_shared<cvedix_face_osd_node_v2>(...);
auto screen = std::make_shared<cvedix_screen_des_node>(...);

detector->attach_to({file_src});
encoder->attach_to({detector});
osd->attach_to({encoder});
screen->attach_to({osd});
```

### Usage
```bash
./build/bin/1-1-1_sample
```

### Use Case
- Demo cơ bản nhất
- Test single inference task
- Hiểu cách pipeline hoạt động

---

## Sample 2: 1-1-N Pipeline

### File
`1-1-N_sample.cpp`

### Mô tả
**1 input → 1 inference → N outputs** (multiple destinations)

### Pipeline Structure
```
Video File → Face Detector → Face Encoder → OSD → [Screen, RTMP, File]
```

### Features
- Một inference task
- Nhiều outputs (screen, RTMP, file)
- Sử dụng `cvedix_split_node` để phân nhánh

### Usage
```bash
./build/bin/1-1-N_sample
```

### Use Case
- Stream đến nhiều destinations
- Record và display đồng thời
- Multi-channel output

---

## Sample 3: 1-N-N Pipeline

### File
`1-N-N_sample.cpp`

### Mô tả
**1 input → N inference tasks → N outputs** (parallel processing)

### Pipeline Structure
```
Video File → [Detector1, Detector2] → [OSD1, OSD2] → [Screen1, Screen2]
```

### Features
- Một input source
- Nhiều inference tasks song song
- Mỗi task có output riêng

### Usage
```bash
./build/bin/1-N-N_sample
```

### Use Case
- Chạy nhiều models song song
- So sánh kết quả giữa các models
- Multi-task processing

---

## Sample 4: N-1-N Pipeline

### File
`N-1-N_sample.cpp`, `1-N-1_sample.cpp`, `1-N-1_sample2.cpp`, `1-N-1_sample3.cpp`

### Mô tả
**N inputs → 1 inference → N outputs** (merge và split)

### Pipeline Structure
```
[Video1, Video2] → Merge → Detector → Split → [Screen1, Screen2]
```

### Features
- Nhiều input sources
- Merge thành một stream
- Một inference task
- Split lại thành nhiều outputs

### Usage
```bash
./build/bin/N-1-N_sample
./build/bin/1-N-1_sample
```

### Use Case
- Multi-camera processing
- Centralized inference
- Multi-display output

---

## Sample 5: N-N Pipeline

### File
`N-N_sample.cpp`

### Mô tả
**N independent pipelines** (mỗi pipeline là 1-1-1 hoặc cấu trúc khác)

### Pipeline Structure
```
Pipeline 1: Video1 → Detector1 → Screen1
Pipeline 2: Video2 → Detector2 → Screen2
Pipeline 3: Video3 → Detector3 → Screen3
...
```

### Features
- Nhiều pipelines độc lập
- Mỗi pipeline có cấu trúc riêng
- Không chia sẻ resources

### Usage
```bash
./build/bin/N-N_sample
```

### Use Case
- Multi-tenant systems
- Independent processing
- Resource isolation

---

## So sánh các kiến trúc

| Pattern | Inputs | Inference | Outputs | Use Case |
|---------|--------|-----------|---------|----------|
| **1-1-1** | 1 | 1 | 1 | Simple processing |
| **1-1-N** | 1 | 1 | N | Multi-destination |
| **1-N-N** | 1 | N | N | Parallel tasks |
| **N-1-N** | N | 1 | N | Multi-camera, centralized |
| **N-N** | N | N | N | Independent pipelines |

---

## Build

```bash
cd build
cmake ..
make 1-1-1_sample
make 1-1-N_sample
make 1-N-N_sample
make N-1-N_sample
make N-N_sample
```

---

## Customization

### Thay đổi input source

```cpp
// File source
auto file_src = std::make_shared<cvedix_file_src_node>(...);

// RTSP source
auto rtsp_src = std::make_shared<cvedix_rtsp_src_node>(...);

// App source (custom data)
auto app_src = std::make_shared<cvedix_app_src_node>(...);
```

### Thay đổi inference node

```cpp
// Face detection
auto detector = std::make_shared<cvedix_yunet_face_detector_node>(...);

// Vehicle detection
auto detector = std::make_shared<cvedix_yolo_detector_node>(...);

// Pose estimation
auto detector = std::make_shared<cvedix_openpose_detector_node>(...);
```

### Thay đổi output destination

```cpp
// Screen
auto screen = std::make_shared<cvedix_screen_des_node>(...);

// RTMP
auto rtmp = std::make_shared<cvedix_rtmp_des_node>(...);

// File
auto file_des = std::make_shared<cvedix_file_des_node>(...);
```

---

## Best Practices

1. **Start với 1-1-1** để hiểu cơ bản
2. **Thêm outputs** với 1-1-N khi cần
3. **Parallel processing** với 1-N-N cho performance
4. **Multi-camera** với N-1-N cho centralized processing
5. **Isolation** với N-N cho multi-tenant

---

## Related Documentation

- [Main README](README.md) - Tổng quan tất cả samples
- [Pipeline Architecture](../doc/PIPELINE_ARCHITECTURE.md) - Chi tiết kiến trúc

