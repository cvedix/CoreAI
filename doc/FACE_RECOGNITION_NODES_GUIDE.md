# CVEDIX Face Recognition & Registration Nodes

Hướng dẫn sử dụng các node nhận diện và đăng ký khuôn mặt trong CVEDIX AI Runtime SDK.

## 📋 Tổng quan

| Node | Chức năng | License |
|------|-----------|---------|
| `cvedix_face_registration_node` | Đăng ký khuôn mặt mới vào database | Không yêu cầu |
| `cvedix_face_recognition_node` | Nhận diện khuôn mặt từ database | ✅ Yêu cầu `face_recognition` |

## 🔧 1. Face Registration Node

### Mô tả
Node đăng ký khuôn mặt với hỗ trợ augmentation tự động. Khi đăng ký, node sẽ tạo nhiều biến thể (kính, khẩu trang, mũ) để tăng độ chính xác nhận diện.

### Constructor

```cpp
#include "cvedix/nodes/infers/cvedix_face_registration_node.h"

cvedix_face_registration_node(
    std::string node_name,           // Tên node
    std::string model_path,          // Đường dẫn model ONNX (InsightFace/ArcFace)
    std::string database_path,       // Đường dẫn lưu database
    bool enable_augmentation = true, // Bật augmentation
    std::string glasses_template = "",// Template kính (PNG)
    std::string mask_template = "",   // Template khẩu trang (PNG)
    std::string hat_template = "",    // Template mũ (PNG)
    int input_width = 112,           // Kích thước input (mặc định 112)
    int input_height = 112
);
```

### API chính

```cpp
// ==================== Registration API ====================

// Đặt tên cho người sắp đăng ký (gọi trước khi xử lý frame)
void set_registration_name(const std::string& person_name);

// Lấy số embeddings đã đăng ký cho lần đăng ký gần nhất
int get_last_registration_count() const;

// ==================== Database API ====================

bool save_database();           // Lưu database ra file
bool load_database();           // Load database từ file
void clear_database();          // Xóa tất cả 
void print_database_stats();    // In thống kê

// Lấy reference đến database
cvedix_face_utils::EnhancedFaceDatabase& get_database();
```

### Ví dụ sử dụng

```cpp
#include "cvedix/nodes/infers/cvedix_face_registration_node.h"
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"

int main() {
    // Tạo pipeline
    auto src = std::make_shared<cvedix_file_src_node>("src", "photo.jpg");
    
    auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
        "detector",
        "face_detection_yunet_2023mar_int8.onnx"
    );
    
    auto registration = std::make_shared<cvedix_face_registration_node>(
        "registration",
        "face_recognition_sface_2021dec.onnx",  // Model
        "./face_database.txt",                   // Database output
        true,                                    // Enable augmentation
        "glasses.png",                           // Optional templates
        "mask.png",
        "hat.png"
    );
    
    // Kết nối pipeline
    src->connect(detector);
    detector->connect(registration);
    
    // Đăng ký một người
    registration->set_registration_name("NGUYEN_VAN_A");
    
    // Xử lý frame
    src->start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    src->stop();
    
    // Lưu database
    registration->save_database();
    registration->print_database_stats();
    
    return 0;
}
```

---

## 🎯 2. Face Recognition Node

### Mô tả
Node nhận diện khuôn mặt với các kỹ thuật nâng cao:

| Technique | Mặc định | Mô tả |
|-----------|----------|-------|
| **Temporal Voting** | ✅ ON | Ổn định kết quả qua nhiều frame |
| **TTA (Test-Time Augmentation)** | ❌ OFF | Tăng độ chính xác (giảm FPS ~3x) |
| **ID-Specific Threshold** | ❌ OFF | Ngưỡng riêng cho từng người |

### Constructor

```cpp
#include "cvedix/nodes/infers/cvedix_face_recognition_node.h"

// Constructor cơ bản (balanced mode)
cvedix_face_recognition_node(
    std::string node_name,
    std::string model_path,
    std::string database_path = "",
    int input_width = 112,
    int input_height = 112,
    bool enable_alignment = true
);

// Constructor nâng cao với config
cvedix_face_recognition_node(
    std::string node_name,
    std::string model_path,
    std::string database_path,
    const cvedix_face_utils::RecognitionConfig& config,
    int input_width = 112,
    int input_height = 112
);
```

### Recognition Configuration

```cpp
using namespace cvedix_face_utils;

// Preset configurations
RecognitionConfig::fast();          // Real-time, voting OFF
RecognitionConfig::balanced();      // Default, voting ON, TTA OFF
RecognitionConfig::high_accuracy(); // All techniques ON

// Custom config
RecognitionConfig config;
config.voting_enabled = true;
config.voting_window_size = 10;
config.voting_majority_threshold = 0.5f;
config.tta.enabled = false;
config.similarity_threshold = 0.7f;
config.confidence_margin = 0.3f;
```

### API chính

```cpp
// ==================== Recognition API ====================

// Lấy kết quả nhận diện mới nhất
MatchResult get_last_match() const;

// Lấy kết quả ổn định từ voting buffer (cho video)
std::pair<std::string, float> get_stable_result(int track_id);

// ==================== Database API ====================

bool load_database(const std::string& path);
EnhancedFaceDatabase& get_database();
void print_database_stats();

// ==================== Configuration API ====================

void set_config(const RecognitionConfig& config);
RecognitionConfig get_config() const;

void set_voting_enabled(bool enabled);
void set_tta_enabled(bool enabled);            // Giảm FPS ~3x
void set_id_threshold_enabled(bool enabled);

// ID-Specific Threshold: ngưỡng riêng cho từng người
void set_personal_threshold(const std::string& name, float threshold);
float get_personal_threshold(const std::string& name) const;

// Legacy configuration
float similarity_threshold = 0.7f;   // Ngưỡng nhận diện
float confidence_margin = 0.3f;       // Margin confidence
bool enable_alignment;                // Bật face alignment
```

### Ví dụ sử dụng

#### Basic Usage (Image)
```cpp
#include "cvedix/nodes/infers/cvedix_face_recognition_node.h"

int main() {
    // Tạo pipeline
    auto src = std::make_shared<cvedix_file_src_node>("src", "test.jpg");
    
    auto detector = std::make_shared<cvedix_yunet_face_detector_node>(
        "detector",
        "face_detection_yunet_2023mar_int8.onnx"
    );
    
    auto recognizer = std::make_shared<cvedix_face_recognition_node>(
        "recognizer",
        "face_recognition_sface_2021dec.onnx",
        "./face_database.txt"
    );
    
    // Kết nối pipeline
    src->connect(detector);
    detector->connect(recognizer);
    
    // Xử lý
    src->start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    src->stop();
    
    // Lấy kết quả
    auto match = recognizer->get_last_match();
    std::cout << "Recognized: " << match.name 
              << " (score: " << match.score << ")" << std::endl;
    
    return 0;
}
```

#### Video Stream với Temporal Voting
```cpp
// Config cho video stream
using namespace cvedix_face_utils;

auto config = RecognitionConfig::balanced();
config.voting_window_size = 15;        // 15 frames
config.voting_majority_threshold = 0.6; // 60% threshold

auto recognizer = std::make_shared<cvedix_face_recognition_node>(
    "recognizer",
    "face_recognition_sface_2021dec.onnx",
    "./face_database.txt",
    config
);

// Trong callback hoặc loop
void on_frame_processed(int track_id) {
    auto [name, score] = recognizer->get_stable_result(track_id);
    if (name != "Unknown") {
        std::cout << "Stable recognition: " << name << std::endl;
    }
}
```

#### High Accuracy Mode với TTA
```cpp
// Bật TTA cho verification scenario (giảm FPS)
auto config = RecognitionConfig::high_accuracy();

auto recognizer = std::make_shared<cvedix_face_recognition_node>(
    "recognizer",
    "face_recognition_sface_2021dec.onnx",
    "./face_database.txt",
    config
);
```

#### ID-Specific Threshold
```cpp
// Đặt ngưỡng khác nhau cho từng người
recognizer->set_id_threshold_enabled(true);

recognizer->set_personal_threshold("CEO", 0.8f);        // Cao hơn cho VIP
recognizer->set_personal_threshold("GUEST", 0.5f);      // Thấp hơn cho khách
```

---

## 📊 Pipeline Flow

```
┌─────────────┐    ┌──────────────────┐    ┌────────────────────────┐
│  File/RTSP  │───▶│  Face Detector   │───▶│  Face Registration     │
│  Source     │    │  (YuNet, SSD)    │    │  (Đăng ký khuôn mặt)   │
└─────────────┘    └──────────────────┘    └────────────────────────┘
                                                      │
                                                      ▼
                                              ┌──────────────┐
                                              │  Database    │
                                              │  (face_db)   │
                                              └──────────────┘
                                                      │
                   ┌──────────────────┐              │
┌─────────────┐    │  Face Detector   │    ┌────────▼───────────────┐
│  File/RTSP  │───▶│  (YuNet, SSD)    │───▶│  Face Recognition      │
│  Source     │    │                  │    │  (Nhận diện khuôn mặt) │
└─────────────┘    └──────────────────┘    └────────────────────────┘
                                                      │
                                                      ▼
                                              ┌──────────────┐
                                              │  OSD/Output  │
                                              └──────────────┘
```

---

## 📁 Database Format

File database là text file đơn giản:
```
NGUYEN_VAN_A|0.123,0.456,0.789,...   # 512 hoặc 128 giá trị
NGUYEN_VAN_A|0.234,0.567,0.890,...   # Nhiều embeddings/người
TRAN_VAN_B|0.345,0.678,0.901,...
```

---

## ⚠️ Lưu ý quan trọng

1. **License**: `cvedix_face_recognition_node` yêu cầu license với feature `face_recognition`
2. **Model compatibility**: Sử dụng cùng model cho cả registration và recognition
3. **Landmarks**: Cần face detector cung cấp 5-point landmarks để alignment
4. **Embedding size**: Model 128-dim (SFace) hoặc 512-dim (ArcFace)

---

## 🔗 Xem thêm

- [InsightFace ONNX Models](https://github.com/deepinsight/insightface)
- [YuNet Face Detector](https://github.com/opencv/opencv_zoo)
- CVEDIX Samples: `samples/face_registration_sample.cpp`, `samples/face_recognition_sample.cpp`
