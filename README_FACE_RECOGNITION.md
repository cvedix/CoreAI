# CVEDIX Face Recognition & Registration Nodes

<p align="center">
  <img src="https://img.shields.io/badge/Version-2025.0.1.3--dev-blue" alt="Version">
  <img src="https://img.shields.io/badge/License-Commercial-green" alt="License">
  <img src="https://img.shields.io/badge/Platform-Linux-orange" alt="Platform">
</p>

Hệ thống nhận diện và đăng ký khuôn mặt trong CVEDIX AI Runtime SDK với các kỹ thuật nâng cao như Temporal Voting, Test-Time Augmentation (TTA), và ID-Specific Threshold.

## 📋 Mục lục

- [Tổng quan](#-tổng-quan)
- [Yêu cầu](#-yêu-cầu)
- [Cài đặt](#-cài-đặt)
- [Quick Start](#-quick-start)
- [Face Registration Node](#-face-registration-node)
- [Face Recognition Node](#-face-recognition-node)
- [Configuration](#-configuration)
- [Database Format](#-database-format)
- [Samples](#-samples)
- [Troubleshooting](#-troubleshooting)

## 🎯 Tổng quan

| Node | Chức năng | License Required |
|------|-----------|------------------|
| `cvedix_face_registration_node` | Đăng ký khuôn mặt mới vào database | ❌ Không |
| `cvedix_face_recognition_node` | Nhận diện khuôn mặt từ database | ✅ Yêu cầu `face_recognition` |

### Tính năng chính

- ✅ **Face Alignment**: Căn chỉnh khuôn mặt sử dụng 5-point landmarks
- ✅ **Temporal Voting**: Ổn định kết quả qua nhiều frame (video)
- ✅ **TTA (Test-Time Augmentation)**: Tăng độ chính xác
- ✅ **ID-Specific Threshold**: Ngưỡng riêng cho từng người
- ✅ **Auto Augmentation**: Tự động tạo biến thể (kính, khẩu trang, mũ)
- ✅ **Multi-embedding per person**: Hỗ trợ nhiều embedding/người

## 📦 Yêu cầu

### Dependencies
- OpenCV 4.6+
- CVEDIX AI Runtime SDK 2025.0.1.3+

### Models (ONNX)
| Model | Input Size | Embedding Dim | Ghi chú |
|-------|------------|---------------|---------|
| `face_recognition_sface_2021dec.onnx` | 112x112 | 128 | Khuyến nghị |
| `w600k_mbf.onnx` | 112x112 | 512 | ArcFace |

### Face Detector
- `face_detection_yunet_2023mar_int8.onnx` (khuyến nghị)
- Bất kỳ detector nào cung cấp 5-point landmarks

## 🔧 Cài đặt

```bash
# Cài đặt CVEDIX AI Runtime
sudo dpkg -i cvedix-ai-runtime-2025.0.1.3-dev-x86_64.deb

# Kiểm tra license
/opt/cvedix/bin/license_info_sample
```

## 🚀 Quick Start

### 1. Đăng ký khuôn mặt

```cpp
#include "cvedix/nodes/infers/cvedix_face_registration_node.h"

// Tạo registration node
auto registration = std::make_shared<cvedix_nodes::cvedix_face_registration_node>(
    "registration",
    "face_recognition_sface_2021dec.onnx",
    "./face_database.txt"
);

// Đặt tên cho người cần đăng ký
registration->set_registration_name("NGUYEN_VAN_A");

// Xử lý frame (sau khi face detector)
// ...

// Lưu database
registration->save_database();
```

### 2. Nhận diện khuôn mặt

```cpp
#include "cvedix/nodes/infers/cvedix_face_recognition_node.h"

// Tạo recognition node
auto recognizer = std::make_shared<cvedix_nodes::cvedix_face_recognition_node>(
    "recognizer",
    "face_recognition_sface_2021dec.onnx",
    "./face_database.txt"
);

// Sau khi xử lý frame
auto match = recognizer->get_last_match();
std::cout << "Nhận diện: " << match.name 
          << " (score: " << match.score << ")" << std::endl;
```

## 📝 Face Registration Node

### Constructor

```cpp
cvedix_face_registration_node(
    std::string node_name,           // Tên node
    std::string model_path,          // Đường dẫn model ONNX
    std::string database_path,       // Đường dẫn lưu database
    bool enable_augmentation = true, // Bật augmentation
    std::string glasses_template = "",// Template kính (PNG)
    std::string mask_template = "",   // Template khẩu trang
    std::string hat_template = "",    // Template mũ
    int input_width = 112,
    int input_height = 112
);
```

### API

| Method | Mô tả |
|--------|-------|
| `set_registration_name(name)` | Đặt tên cho người cần đăng ký |
| `get_last_registration_count()` | Số embeddings đã đăng ký |
| `save_database()` | Lưu database ra file |
| `load_database()` | Load database (để append) |
| `clear_database()` | Xóa toàn bộ database |
| `print_database_stats()` | In thống kê |

### Ví dụ

```cpp
// Đăng ký nhiều ảnh cho cùng một người
for (const auto& photo : photos) {
    registration->set_registration_name("NGUYEN_VAN_A");
    process_frame(photo);  // Qua pipeline
}
registration->save_database();
```

## 🎯 Face Recognition Node

### Constructor

```cpp
// Cơ bản (balanced mode)
cvedix_face_recognition_node(
    std::string node_name,
    std::string model_path,
    std::string database_path = "",
    int input_width = 112,
    int input_height = 112,
    bool enable_alignment = true
);

// Nâng cao với config
cvedix_face_recognition_node(
    std::string node_name,
    std::string model_path,
    std::string database_path,
    const RecognitionConfig& config,
    int input_width = 112,
    int input_height = 112
);
```

### API

| Method | Mô tả |
|--------|-------|
| `get_last_match()` | Kết quả nhận diện gần nhất |
| `get_stable_result(track_id)` | Kết quả ổn định từ voting |
| `load_database(path)` | Load database |
| `set_config(config)` | Đặt config |
| `set_voting_enabled(bool)` | Bật/tắt voting |
| `set_tta_enabled(bool)` | Bật/tắt TTA |
| `set_personal_threshold(name, threshold)` | Ngưỡng riêng cho người |

### Kết quả nhận diện

```cpp
struct MatchResult {
    std::string name;    // Tên người (hoặc "Unknown")
    float score;         // Điểm similarity (0.0 - 1.0)
    bool confident;      // Có confident không
};

auto match = recognizer->get_last_match();
if (match.confident) {
    std::cout << "Matched: " << match.name << std::endl;
}
```

## ⚙️ Configuration

### Preset Configs

```cpp
using namespace cvedix_face_utils;

// Real-time (FPS cao nhất)
auto config = RecognitionConfig::fast();

// Cân bằng (mặc định, voting ON)
auto config = RecognitionConfig::balanced();

// Độ chính xác cao (tất cả kỹ thuật ON)
auto config = RecognitionConfig::high_accuracy();
```

### Custom Config

```cpp
RecognitionConfig config;
config.voting_enabled = true;
config.voting_window_size = 10;          // 10 frames
config.voting_majority_threshold = 0.5f;  // 50%
config.tta.enabled = false;
config.tta.use_flip = true;
config.tta.use_brightness = false;
config.similarity_threshold = 0.7f;
config.confidence_margin = 0.3f;
config.id_specific_threshold_enabled = false;
```

### So sánh các mode

| Mode | Voting | TTA | FPS | Độ chính xác |
|------|--------|-----|-----|--------------|
| `fast()` | ❌ OFF | ❌ OFF | ⭐⭐⭐ | ⭐⭐ |
| `balanced()` | ✅ ON | ❌ OFF | ⭐⭐ | ⭐⭐⭐ |
| `high_accuracy()` | ✅ ON | ✅ ON | ⭐ | ⭐⭐⭐⭐ |

## 📁 Database Format

File text đơn giản (CSV-like):

```
NAME|embedding_value_1,embedding_value_2,...,embedding_value_N
```

Ví dụ:
```
NGUYEN_VAN_A|-0.097844,0.086444,0.160233,...
NGUYEN_VAN_A|-0.006704,0.108889,-0.044868,...
TRAN_VAN_B|0.060296,0.037590,0.016923,...
```

**Lưu ý:** Một người có thể có nhiều embeddings (từ nhiều ảnh hoặc augmentation).

## 📚 Samples

### Sample files

| File | Mô tả |
|------|-------|
| `samples/face_registration_sample.cpp` | Đăng ký khuôn mặt từ ảnh |
| `samples/face_recognition_sample.cpp` | Nhận diện từ ảnh/video |

### Chạy samples

```bash
cd /opt/cvedix/bin

# Đăng ký khuôn mặt
./face_registration_sample ./photo.jpg "NGUYEN_VAN_A"

# Nhận diện
./face_recognition_sample ./test.jpg
./face_recognition_sample ./video.mp4
```

## 🔍 Troubleshooting

### 1. License Error

```
Error: Face recognition features require a valid license.
```

**Giải pháp:** Cài đặt license với feature `face_recognition`:
```bash
./license_generator private_key.pem license.lic 2025-12-31 "tensorrt,rknn,face_recognition"
```

### 2. Unknown Detection

```
Recognized: Unknown (score: 0.1234)
```

**Giải pháp:**
- Đăng ký thêm ảnh với góc/ánh sáng khác nhau
- Giảm `similarity_threshold` (mặc định 0.7)
- Kiểm tra model compatibility (cùng model cho registration và recognition)

### 3. Embedding Size Mismatch

**Giải pháp:** Đảm bảo cùng model:
- Registration: `face_recognition_sface_2021dec.onnx` (128-dim)
- Recognition: `face_recognition_sface_2021dec.onnx` (128-dim)

### 4. No Face Detected

**Giải pháp:**
- Kiểm tra face detector output
- Đảm bảo detector cung cấp 5-point landmarks
- Giảm score threshold của detector

## 📊 Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    REGISTRATION PIPELINE                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌────────┐   ┌───────────┐   ┌───────────────────┐        │
│  │ Image  │──▶│  Face     │──▶│ Face Registration │        │
│  │ Source │   │ Detector  │   │ Node              │        │
│  └────────┘   └───────────┘   └─────────┬─────────┘        │
│                                         │                   │
│                                         ▼                   │
│                               ┌─────────────────┐           │
│                               │ face_database   │           │
│                               │ .txt            │           │
│                               └─────────────────┘           │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    RECOGNITION PIPELINE                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                               ┌─────────────────┐           │
│                               │ face_database   │           │
│                               │ .txt            │           │
│                               └────────┬────────┘           │
│                                        │                    │
│  ┌────────┐   ┌───────────┐   ┌───────▼───────────┐        │
│  │ Image/ │──▶│  Face     │──▶│ Face Recognition  │        │
│  │ Video  │   │ Detector  │   │ Node              │        │
│  └────────┘   └───────────┘   └─────────┬─────────┘        │
│                                         │                   │
│                                         ▼                   │
│                               ┌─────────────────┐           │
│                               │ OSD / Output    │           │
│                               └─────────────────┘           │
└─────────────────────────────────────────────────────────────┘
```

## 📄 License

CVEDIX AI Runtime SDK là phần mềm thương mại. 

- **Face Registration**: Không yêu cầu license
- **Face Recognition**: Yêu cầu license với feature `face_recognition`

---

<p align="center">
  <b>CVEDIX Corporation</b><br>
  <a href="mailto:support@cvedix.com">support@cvedix.com</a>
</p>
