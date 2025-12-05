# Utility Samples

## Tổng quan

Các samples này demo các utility functions và advanced features của framework.

---

## Sample 1: Logger

### File
`cvedix_logger_sample.cpp`

### Mô tả
Demo cách sử dụng `cvedix_logger` để logging.

### Features
- Multiple log levels (DEBUG, INFO, WARN, ERROR)
- Console và file output
- Kafka logging support
- Thread-safe logging
- Configurable log format

### Usage
```bash
./build/bin/cvedix_logger_sample
```

### Configuration

#### Log Level
```cpp
CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::DEBUG);
CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::ERROR);
```

#### Log Content
```cpp
// Include/exclude components
CVEDIX_SET_LOG_INCLUDE_THREAD_ID(true);      // Include thread ID
CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(true);  // Include file:line
CVEDIX_SET_LOG_INCLUDE_LEVEL(true);          // Include log level
```

#### Log Output
```cpp
// Output destinations
CVEDIX_SET_LOG_TO_CONSOLE(true);   // Console output
CVEDIX_SET_LOG_TO_FILE(true);      // File output
CVEDIX_SET_LOG_TO_KAFKA(true);     // Kafka output (if enabled)
```

#### Log Directory
```cpp
CVEDIX_SET_LOG_DIR("./log");  // Set log directory
```

#### Kafka Configuration
```cpp
CVEDIX_SET_LOG_TO_KAFKA(true);
CVEDIX_SET_LOG_KAFKA_SERVERS_AND_TOPIC("192.168.1.100:9092/cvedix_log");
```

### Logging Macros
```cpp
CVEDIX_DEBUG("Debug message");
CVEDIX_INFO("Info message");
CVEDIX_WARN("Warning message");
CVEDIX_ERROR("Error message");
```

### Example
```cpp
CVEDIX_LOGGER_INIT();

// Log from multiple threads
std::thread t1([]() {
    for (int i = 0; i < 100; i++) {
        CVEDIX_INFO("Thread 1: " + std::to_string(i));
    }
});

std::thread t2([]() {
    for (int i = 0; i < 100; i++) {
        CVEDIX_DEBUG("Thread 2: " + std::to_string(i));
    }
});
```

---

## Sample 2: Record Node

### File
`record_sample.cpp`

### Mô tả
Demo cách sử dụng `cvedix_record_node` để record video và images.

### Pipeline
```
[Video1, Video2] → Detector → Tracker → OSD → Recorder → Split → [Screen1, Screen2]
```

### Features
- Video recording
- Image recording
- Manual trigger
- Automatic trigger (via hooks)
- Multi-channel support

### Usage
```bash
./build/bin/record_sample
```

### Configuration
```cpp
auto recorder = std::make_shared<cvedix_record_node>(
    "recorder",
    "./record/video",  // Video output directory
    "./record/image"   // Image output directory
);
```

### Manual Recording
```cpp
// Record video manually
file_src->record_video_manually(0, 10.0);  // Channel 0, 10 seconds

// Record image manually
file_src->record_image_manually(0);  // Channel 0, current frame
```

### Recording Hooks
```cpp
// Set hooker for recording completion
auto record_hooker = [](int channel, cvedix_record_info record_info) {
    auto record_type = record_info.record_type == 
        cvedix_nodes::cvedix_record_type::IMAGE ? "image" : "video";
    
    std::cout << "Channel [" << channel << "] [" << record_type 
              << "] record completed! Path: " 
              << record_info.full_record_path << std::endl;
};

recorder->set_image_record_complete_hooker(record_hooker);
recorder->set_video_record_complete_hooker(record_hooker);
```

### Recording Triggers
```cpp
// Trigger recording from external code
// (Implementation depends on use case)
```

---

## Sample 3: Interaction with Pipeline

### File
`interaction_with_pipe_sample.cpp`

### Mô tả
Demo cách tương tác với pipeline, như start/stop channel bằng API.

### Features
- Start/stop channels
- Dynamic pipeline control
- API-based interaction

### Usage
```bash
./build/bin/interaction_with_pipe_sample
```

### API Examples
```cpp
// Start channel
file_src->start();

// Stop channel
file_src->stop();

// Detach pipeline
file_src->detach_recursively();

// Get channel status
bool is_running = file_src->is_running();
```

---

## Sample 4: Dynamic Pipeline

### Files
- `dynamic_pipeline_sample.cpp`
- `dynamic_pipeline_sample2.cpp`

### Mô tả
Demo cách tạo và quản lý pipeline động.

### Features
- Dynamic node creation
- Runtime pipeline modification
- Flexible architecture

### Usage
```bash
./build/bin/dynamic_pipeline_sample
./build/bin/dynamic_pipeline_sample2
```

---

## Sample 5: Skip Node

### File
`skip_sample.cpp`

### Mô tả
Demo cách sử dụng skip node để bỏ qua một số frames.

### Pipeline
```
Video → Skip → Detector → OSD → Screen
```

### Features
- Frame skipping
- Performance optimization
- Configurable skip rate

### Usage
```bash
./build/bin/skip_sample
```

### Configuration
```cpp
auto skip = std::make_shared<cvedix_skip_node>(
    "skip",
    2  // Skip every 2 frames (process 1 out of 3)
);
```

---

## Sample 6: Frame Fusion

### File
`frame_fusion_sample.cpp`

### Mô tả
Demo cách fusion frames từ nhiều sources.

### Pipeline
```
[Source1, Source2] → Fusion → Detector → OSD → Screen
```

### Features
- Multi-source fusion
- Frame alignment
- Synchronization

### Usage
```bash
./build/bin/frame_fusion_sample
```

---

## Sample 7: Multi Detectors

### Files
- `multi_detectors_sample.cpp`
- `multi_detectors_and_classifiers_sample.cpp`

### Mô tả
Demo cách sử dụng nhiều detectors song song.

### Pipeline
```
Video → [Detector1, Detector2, ...] → Merge → OSD → Screen
```

### Features
- Parallel detection
- Multiple models
- Result fusion

### Usage
```bash
./build/bin/multi_detectors_sample
./build/bin/multi_detectors_and_classifiers_sample
```

---

## Sample 8: Plate Recognition

### File
`plate_recognize_sample.cpp`

### Mô tả
Vehicle plate detection và recognition trên toàn frame (không cần detect vehicle trước).

### Pipeline
```
Video → Plate Detector → Plate Recognizer → OSD → Screen
```

### Features
- Direct plate detection
- OCR recognition
- No vehicle detection needed

### Usage
```bash
./build/bin/plate_recognize_sample
```

---

## Sample 9: Video Restoration

### File
`video_restoration_sample.cpp`

### Mô tả
Video restoration/enhancement sử dụng AI models.

### Pipeline
```
Video → Restoration → OSD → Screen
```

### Features
- Video enhancement
- Quality improvement
- Real-time processing

### Usage
```bash
./build/bin/video_restoration_sample
```

---

## Sample 10: NV Hardware Codec

### File
`nv_hard_codec_sample.cpp`

### Mô tả
Sử dụng NVIDIA hardware codec cho encoding/decoding.

### Pipeline
```
Video → NV Decoder → Detector → NV Encoder → Output
```

### Features
- Hardware acceleration
- Low latency
- High performance

### Usage
```bash
./build/bin/nv_hard_codec_sample
```

### Requirements
- NVIDIA GPU
- NVENC/NVDEC support
- Build với `-DCVEDIX_WITH_NVCODEC=ON`

---

## Sample 11: Paddle Inference

### File
`paddle_infer_sample.cpp`

### Mô tả
OCR sử dụng PaddlePaddle inference.

### Pipeline
```
Video → OCR (Paddle) → OSD → [Screen, RTMP]
```

### Features
- PaddlePaddle inference
- OCR recognition
- Multiple outputs

### Usage
```bash
./build/bin/paddle_infer_sample
```

### Requirements
- PaddlePaddle installed
- Build với `-DCVEDIX_WITH_PADDLE=ON`

---

## Sample 12: Multi-LLM Analysis

### Files
- `mllm_analyse_sample.cpp`
- `mllm_analyse_sample_openai.cpp`

### Mô tả
Multi-modal LLM analysis cho video/images.

### Pipeline
```
Video → Detector → LLM Analysis → Output
```

### Features
- LLM integration
- Multi-modal analysis
- OpenAI API support

### Usage
```bash
./build/bin/mllm_analyse_sample
./build/bin/mllm_analyse_sample_openai
```

### Requirements
- LLM API access
- Build với appropriate flags

---

## So sánh các Utility Samples

| Sample | Chức năng | Use Case |
|--------|-----------|----------|
| **cvedix_logger** | Logging | Debug, monitoring |
| **record** | Recording | Video/image capture |
| **interaction_with_pipe** | Pipeline control | Dynamic control |
| **dynamic_pipeline** | Dynamic pipeline | Flexible architecture |
| **skip** | Frame skipping | Performance |
| **frame_fusion** | Frame fusion | Multi-source |
| **multi_detectors** | Parallel detection | Multi-task |
| **plate_recognize** | Plate OCR | Traffic monitoring |
| **video_restoration** | Video enhancement | Quality improvement |
| **nv_hard_codec** | Hardware codec | Performance |
| **paddle_infer** | PaddlePaddle | OCR |
| **mllm_analyse** | LLM analysis | AI analysis |

---

## Build

### Basic Utilities
```bash
cd build
cmake ..
make cvedix_logger_sample
make record_sample
make interaction_with_pipe_sample
```

### Advanced Features
```bash
# NV Codec
cmake -DCVEDIX_WITH_NVCODEC=ON ..
make nv_hard_codec_sample

# PaddlePaddle
cmake -DCVEDIX_WITH_PADDLE=ON ..
make paddle_infer_sample
```

---

## Use Cases

### 1. Debug & Monitoring
```bash
# Enable logging
./build/bin/cvedix_logger_sample
```

### 2. Recording System
```bash
# Record video/images
./build/bin/record_sample
```

### 3. Dynamic Control
```bash
# Control pipeline via API
./build/bin/interaction_with_pipe_sample
```

### 4. Performance Optimization
```bash
# Skip frames for performance
./build/bin/skip_sample
```

---

## Related Documentation

- [Main README](README.md) - Tổng quan samples
- [Basic Pipeline Samples](README_BASIC_PIPELINE_SAMPLES.md) - Pipeline patterns


