# Source & Destination Samples

## Tổng quan

Các samples này demo các loại input sources và output destinations trong pipeline.

---

## Source Samples

### Sample 1: RTSP Source

### File
`rtsp_src_sample.cpp`

### Mô tả
Đọc video stream từ RTSP camera/server.

### Pipeline
```
RTSP Stream → Face Detector → Face Encoder → OSD → Screen
```

### Features
- RTSP stream input
- Support stream switching/restarting
- Auto-reconnection
- Dynamic resolution/fps detection

### Usage
```bash
./build/bin/rtsp_src_sample
```

### Configuration
```cpp
auto rtsp_src = std::make_shared<cvedix_rtsp_src_node>(
    "rtsp_src",
    0,                                      // channel index
    "rtsp://camera_ip:554/stream",         // RTSP URL
    0.6,                                    // playback speed
    true,                                   // auto reconnect
    "tcp"                                   // transport (tcp/udp)
);
```

### RTSP URL Format
```
rtsp://username:password@ip:port/path
rtsp://192.168.1.100:554/live/mainstream
rtsp://admin:123456@192.168.1.100:554/Streaming/Channels/101
```

### Dynamic Stream Info
```cpp
// Get stream information
int width = rtsp_src->get_original_width();
int height = rtsp_src->get_original_height();
float fps = rtsp_src->get_original_fps();
```

---

### Sample 2: Image Source

### File
`image_src_sample.cpp`

### Mô tả
Đọc/receive ảnh từ local file hoặc remote via UDP.

### Pipeline
```
Image Source → Detector → OSD → Screen
```

### Features
- Local file input
- UDP network input
- Single image processing
- Batch image processing

### Usage
```bash
./build/bin/image_src_sample
```

### Configuration
```cpp
// Local file
auto image_src = std::make_shared<cvedix_image_src_node>(
    "image_src",
    0,
    "./cvedix_data/test_images/faces/"
);

// UDP network
auto image_src = std::make_shared<cvedix_image_src_node>(
    "image_src",
    0,
    "udp://0.0.0.0:8888"  // UDP endpoint
);
```

---

### Sample 3: App Source

### File
`app_src_sample.cpp`

### Mô tả
Send data vào pipeline từ host code sử dụng `app_src_node`.

### Pipeline
```
App Source → Detector → OSD → Screen
```

### Features
- Programmatic input
- Custom data injection
- Real-time data push

### Usage
```bash
./build/bin/app_src_sample
```

### Configuration
```cpp
auto app_src = std::make_shared<cvedix_app_src_node>(
    "app_src",
    0  // channel index
);

// Push frame manually
cv::Mat frame = cv::imread("image.jpg");
auto frame_meta = std::make_shared<cvedix_frame_meta>();
frame_meta->frame = frame;
app_src->push_frame_meta(frame_meta);
```

---

### Sample 4: RTMP Source

### File
`rtmp_src_sample.cpp`

### Mô tả
Đọc video stream từ RTMP server.

### Pipeline
```
RTMP Stream → Detector → OSD → Screen
```

### Features
- RTMP stream input
- Live stream support
- Auto-reconnection

### Usage
```bash
./build/bin/rtmp_src_sample
```

### Configuration
```cpp
auto rtmp_src = std::make_shared<cvedix_rtmp_src_node>(
    "rtmp_src",
    0,
    "rtmp://server/live/stream"
);
```

---

## Destination Samples

### Sample 1: RTSP Destination

### File
`rtsp_des_sample.cpp`

### Mô tả
Push video stream qua RTSP, không cần RTSP server, có thể truy cập trực tiếp.

### Pipeline
```
Video → Detector → OSD → RTSP Des
```

### Features
- RTSP server built-in
- No external server needed
- Direct access via RTSP URL

### Usage
```bash
./build/bin/rtsp_des_sample
```

### Access
```bash
# Access stream from another machine
ffplay rtsp://server_ip:8554/stream
vlc rtsp://server_ip:8554/stream
```

### Configuration
```cpp
auto rtsp_des = std::make_shared<cvedix_rtsp_des_node>(
    "rtsp_des",
    0,                      // channel index
    "stream",               // stream name
    8554                    // RTSP port
);
```

---

### Sample 2: RTMP Destination

### File
`rtmp_des_sample.cpp` (trong các samples khác)

### Mô tả
Push video stream qua RTMP đến server.

### Pipeline
```
Video → Detector → OSD → RTMP Des
```

### Features
- RTMP streaming
- Live broadcast
- Server compatibility

### Configuration
```cpp
auto rtmp_des = std::make_shared<cvedix_rtmp_des_node>(
    "rtmp_des",
    0,                                      // channel index
    "rtmp://server:1935/live/stream",      // RTMP URL
    cvedix_objects::cvedix_size{1280, 720}, // output size
    2048                                    // bitrate (kbps)
);
```

---

### Sample 3: Image Destination

### File
`image_des_sample.cpp`

### Mô tả
Save/push ảnh đến local file hoặc remote via UDP.

### Pipeline
```
Video → Detector → OSD → Image Des
```

### Features
- Save to local file
- Push via UDP
- Single image output
- Batch image output

### Usage
```bash
./build/bin/image_des_sample
```

### Configuration
```cpp
// Save to local file
auto image_des = std::make_shared<cvedix_image_des_node>(
    "image_des",
    0,
    "./output/images/",
    "jpg",      // format
    1.0         // save interval (seconds)
);

// Push via UDP
auto image_des = std::make_shared<cvedix_image_des_node>(
    "image_des",
    0,
    "udp://192.168.1.100:8888"
);
```

---

### Sample 4: App Destination

### File
`app_des_sample.cpp`

### Mô tả
Receive data từ pipeline trong host code sử dụng `app_des_node`.

### Pipeline
```
Video → Detector → OSD → App Des
```

### Features
- Programmatic output
- Custom data processing
- Real-time data pull

### Usage
```bash
./build/bin/app_des_sample
```

### Configuration
```cpp
auto app_des = std::make_shared<cvedix_app_des_node>(
    "app_des",
    0  // channel index
);

// Pull frame manually
auto frame_meta = app_des->pull_frame_meta();
if (frame_meta) {
    // Process frame_meta
    cv::Mat frame = frame_meta->frame;
    // ... your processing
}
```

---

### Sample 5: App Source & Destination

### File
`app_src_des_sample.cpp`

### Mô tả
Demo cả app source và app destination trong cùng pipeline.

### Pipeline
```
App Source → Detector → OSD → App Des
```

### Features
- Full programmatic control
- Custom input/output
- Integration với external systems

### Usage
```bash
./build/bin/app_src_des_sample
```

---

### Sample 6: File Source & Destination

### File
`src_des_sample.cpp`

### Mô tả
Demo multiple sources (file, RTSP, UDP) merge vào 1 inference task, sau đó split thành nhiều outputs (screen, RTMP, fake).

### Pipeline
```
[File, RTSP, UDP] → Merge → Detector → Split → [Screen, RTMP, Fake]
```

### Features
- Multiple input sources
- Centralized processing
- Multiple output destinations

### Usage
```bash
./build/bin/src_des_sample
```

---

### Sample 7: FFmpeg Source & Destination

### Files
- `ffmpeg_src_des_sample.cpp`
- `ffmpeg_transcode_sample.cpp`

### Mô tả
Sử dụng FFmpeg cho input/output với nhiều codec và format support.

### Pipeline
```
FFmpeg Source → Detector → OSD → FFmpeg Des
```

### Features
- Multiple codec support
- Format conversion
- Hardware acceleration
- Transcoding

### Usage
```bash
./build/bin/ffmpeg_src_des_sample
./build/bin/ffmpeg_transcode_sample
```

### Requirements
- Build với `-DCVEDIX_WITH_FFMPEG=ON`
- FFmpeg libraries installed

---

## So sánh Sources

| Source | Type | Use Case | Latency |
|--------|------|----------|---------|
| **file_src** | Local file | Testing, offline | Low |
| **rtsp_src** | RTSP stream | IP camera | Medium |
| **rtmp_src** | RTMP stream | Live stream | Medium |
| **image_src** | Image file/UDP | Image processing | Low |
| **app_src** | Programmatic | Custom integration | Low |

---

## So sánh Destinations

| Destination | Type | Use Case | Latency |
|-------------|------|----------|---------|
| **screen_des** | Display | Visualization | Low |
| **file_des** | Local file | Recording | Low |
| **rtsp_des** | RTSP server | Streaming | Medium |
| **rtmp_des** | RTMP server | Broadcasting | Medium |
| **image_des** | Image file/UDP | Image export | Low |
| **app_des** | Programmatic | Custom processing | Low |

---

## Build

```bash
cd build
cmake ..
make rtsp_src_sample
make rtsp_des_sample
make image_src_sample
make image_des_sample
make app_src_sample
make app_des_sample
make app_src_des_sample
make src_des_sample
```

### FFmpeg Samples
```bash
cmake -DCVEDIX_WITH_FFMPEG=ON ..
make ffmpeg_src_des_sample
make ffmpeg_transcode_sample
```

---

## Use Cases

### 1. IP Camera Processing
```bash
# Process RTSP stream
./build/bin/rtsp_src_sample
```

### 2. Live Broadcasting
```bash
# Stream to RTMP server
# (Add rtmp_des to pipeline)
```

### 3. Image Batch Processing
```bash
# Process images from directory
./build/bin/image_src_sample
```

### 4. Custom Integration
```bash
# Programmatic control
./build/bin/app_src_des_sample
```

### 5. Multi-source Processing
```bash
# Multiple inputs, multiple outputs
./build/bin/src_des_sample
```

---

## Configuration Examples

### Example 1: RTSP Input với Authentication
```cpp
auto rtsp_src = std::make_shared<cvedix_rtsp_src_node>(
    "rtsp_src",
    0,
    "rtsp://admin:password@192.168.1.100:554/stream"
);
```

### Example 2: Multiple Outputs
```cpp
auto split = std::make_shared<cvedix_split_node>("split", false);

auto screen = std::make_shared<cvedix_screen_des_node>("screen", 0);
auto rtmp = std::make_shared<cvedix_rtmp_des_node>("rtmp", 0, "rtmp://server/stream");
auto file = std::make_shared<cvedix_file_des_node>("file", 0, "./output.mp4");

osd->attach_to({detector});
split->attach_to({osd});
screen->attach_to({split});
rtmp->attach_to({split});
file->attach_to({split});
```

### Example 3: Image Save với Interval
```cpp
auto image_des = std::make_shared<cvedix_image_des_node>(
    "image_des",
    0,
    "./output/",
    "jpg",
    5.0  // Save every 5 seconds
);
```

---

## Performance Tips

1. **Use hardware acceleration** cho RTSP/RTMP
2. **Adjust buffer size** cho network streams
3. **Batch processing** cho images
4. **Async I/O** cho file operations

---

## Troubleshooting

### Issue: RTSP connection failed
- Check network connectivity
- Verify RTSP URL format
- Check authentication
- Try TCP transport instead of UDP

### Issue: RTMP streaming failed
- Verify RTMP server URL
- Check server authentication
- Adjust bitrate
- Check network bandwidth

### Issue: Image save slow
- Use async I/O
- Reduce save frequency
- Optimize image format/quality

---

## Related Documentation

- [Main README](README.md) - Tổng quan samples
- [Basic Pipeline Samples](README_BASIC_PIPELINE_SAMPLES.md) - Pipeline patterns




