# Hướng dẫn sử dụng `cvedix_face_recognizer_node`

## Tổng quan

`cvedix_face_recognizer_node` là node nhận dạng khuôn mặt all-in-one sử dụng SeetaFace6. Node thực hiện toàn bộ pipeline trong một bước:

```
Detect → Mask Check → Landmarks → Feature Extract → Database Match
```

**Đặc điểm:**
- **Dual-model**: Tự động phát hiện khẩu trang → chuyển sang model mask-specific
- **Auto-load database**: Tự động load database khi khởi tạo (nếu file tồn tại)
- **Thread-safe**: Mutex bảo vệ tất cả thao tác SeetaFace6
- **Zero-copy**: Chuyển đổi `cv::Mat` → `SeetaImageData` không copy dữ liệu

---

## Yêu cầu

### 1. Build SeetaFace6

```bash
cd third_party/seetaface6
bash build_seetaface6.sh
```

### 2. Download model files

Download và đặt vào một thư mục (ví dụ: `/opt/seetaface6/models/`):

| File | Kích thước | Vai trò |
|------|-----------|---------|
| `face_detector.csta` | ~5MB | Phát hiện khuôn mặt |
| `mask_detector.csta` | ~2MB | Phát hiện khẩu trang |
| `face_landmarker_pts5.csta` | ~1MB | 5-point landmarks (standard) |
| `face_landmarker_mask_pts5.csta` | ~1MB | 5-point landmarks (mask) |
| `face_recognizer.csta` | ~100MB | Feature extraction (standard) |
| `face_recognizer_mask.csta` | ~100MB | Feature extraction (mask) |

### 3. Build SDK với SeetaFace6

```bash
mkdir -p build && cd build
cmake .. -DCVEDIX_WITH_SEETAFACE=ON
make -j$(nproc)
```

> [!NOTE]
> CMake tự động phát hiện SeetaFace6 nếu đã build xong. Không cần cấu hình thêm.

---

## Sử dụng cơ bản

### Khởi tạo node

```cpp
#include "cvedix/nodes/infers/cvedix_face_recognizer_node.h"

// Tạo node — chỉ cần 2 tham số: tên + thư mục model
auto face_recognizer = std::make_shared<cvedix_nodes::cvedix_face_recognizer_node>(
    "face_recognizer",           // tên node
    "/opt/seetaface6/models"     // thư mục chứa model files
);
```

### Với auto-load database

```cpp
auto face_recognizer = std::make_shared<cvedix_nodes::cvedix_face_recognizer_node>(
    "face_recognizer",
    "/opt/seetaface6/models",
    "/data/facedb",              // tự động load facedb.std + facedb.mask nếu tồn tại
    0.70f,                       // similarity threshold (0.0 - 1.0)
    40                           // min face size (pixels)
);
```

### Tham số Constructor

| Tham số | Kiểu | Mặc định | Mô tả |
|---------|------|----------|-------|
| `node_name` | `string` | *(bắt buộc)* | Tên node |
| `model_dir` | `string` | *(bắt buộc)* | Thư mục chứa 6 model files |
| `db_path` | `string` | `""` | Đường dẫn base cho database (auto-load) |
| `similarity_threshold` | `float` | `0.70` | Ngưỡng so khớp (0.0 - 1.0) |
| `min_face_size` | `int` | `40` | Kích thước mặt tối thiểu (pixel) |

---

## Pipeline mẫu

### 1. Detect + Track + Recognition

```cpp
#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_recognizer_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // ═══ Nodes ═══
    auto src = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
        "cam_01", 0, "rtsp://192.168.1.100:554/stream1", 0.6);

    auto face = std::make_shared<cvedix_nodes::cvedix_face_recognizer_node>(
        "face_recognizer",
        "/opt/seetaface6/models",
        "/data/facedb",       // auto-load database
        0.70f                 // similarity threshold
    );

    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(
        "face_tracker", cvedix_nodes::cvedix_track_for::FACE);

    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd");
    auto des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    // ═══ Pipeline ═══
    face->attach_to({src});
    tracker->attach_to({face});
    osd->attach_to({tracker});
    des->attach_to({osd});

    // ═══ Start ═══
    src->start();

    cvedix_utils::cvedix_analysis_board board({src});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);
    src->detach_recursively();
}
```

```
Pipeline:
src(RTSP) → face_recognizer → tracker(FACE) → osd_v2 → screen
```

### 2. Đăng ký + Nhận dạng khuôn mặt

```cpp
// Đăng ký khuôn mặt (chạy 1 lần)
cv::Mat photo_john = cv::imread("/photos/john.jpg");
cv::Mat photo_alice = cv::imread("/photos/alice.jpg");

face->registerFace(photo_john, "John");
face->registerFace(photo_alice, "Alice");

// Lưu database (để auto-load lần sau)
face->saveDatabase("/data/facedb");

// Kết quả runtime: frame_meta->face_targets[i]->identify = "John"
```

> [!IMPORTANT]
> Ảnh đăng ký nên **không đeo khẩu trang**, chất lượng tốt, mặt ≥ 150px. Node sẽ tự extract features bằng CẢ 2 model (standard + mask).

---

## Face Database API

| Method | Mô tả | Return |
|--------|-------|--------|
| `registerFace(image, name)` | Đăng ký 1 khuôn mặt (cả 2 model) | `int64_t` index, -1 nếu thất bại |
| `deleteFace(std_id, mask_id)` | Xóa face khỏi cả 2 DB | `bool` |
| `clearDatabase()` | Xóa toàn bộ database | `void` |
| `getDatabaseSize()` | Số lượng faces đã đăng ký | `size_t` |
| `saveDatabase(path)` | Lưu ra file (.std + .mask) | `bool` |
| `loadDatabase(path)` | Load từ file | `bool` |
| `setDatabaseEnabled(on)` | Bật/tắt matching | `void` |
| `setSimilarityThreshold(t)` | Đặt ngưỡng (0.0-1.0) | `void` |
| `setMinFaceSize(size)` | Đặt kích thước mặt tối thiểu | `void` |

### Cấu trúc file database

```
/data/facedb.std         ← Binary features (standard model)
/data/facedb.std.names   ← Text mapping: index → name
/data/facedb.mask        ← Binary features (mask model)
/data/facedb.mask.names  ← Text mapping: index → name
```

---

## Output: `cvedix_frame_face_target`

Mỗi khuôn mặt phát hiện được lưu trong `frame_meta->face_targets[]`:

| Field | Kiểu | Mô tả |
|-------|------|-------|
| `x, y, width, height` | `int` | Bounding box |
| `score` | `float` | Detection confidence |
| `key_points` | `vector<pair<int,int>>` | 5 facial landmarks |
| `embeddings` | `vector<float>` | Feature vector (512/1024-d) |
| `identify` | `string` | Tên người (rỗng nếu không match) |
| `identify_score` | `float` | Similarity score (0.0-1.0) |

---

## Dual-Model Pipeline

Node tự động chuyển đổi giữa 2 pipeline dựa trên phát hiện khẩu trang:

```
                        Detect face
                            │
                  MaskDetector: đeo khẩu trang?
                     /                    \
                   YES                     NO
                    │                       │
       landmarker_mask_pts5        landmarker_pts5
                    │                       │
       recognizer_mask                recognizer
                    │                       │
              Query DB Mask          Query DB Standard
                    │                       │
                    └────── identity ───────┘
```

**Khi đăng ký**: Extract features bằng CẢ 2 model → đăng ký vào CẢ 2 database.
**Khi nhận diện**: Auto-detect mask → route đến model phù hợp → query đúng database.

---

## Cấu hình nâng cao

### Điều chỉnh ngưỡng similarity

```cpp
// Giảm threshold = dễ match hơn (nhiều false positive hơn)
face->setSimilarityThreshold(0.60f);

// Tăng threshold = khó match hơn (ít false positive)
face->setSimilarityThreshold(0.80f);
```

**Khuyến nghị:**
| Môi trường | Threshold |
|-----------|-----------|
| Văn phòng (ánh sáng tốt) | 0.70 - 0.80 |
| Ngoài trời (ánh sáng biến đổi) | 0.60 - 0.70 |
| Bảo mật cao | 0.80 - 0.90 |

### Tăng tốc bằng face size filter

```cpp
// Bỏ qua mặt nhỏ (xa camera) → giảm compute
face->setMinFaceSize(80);  // chỉ detect mặt >= 80px
```

---

## So sánh với các Face Node khác

| Feature | `face_recognizer` (SeetaFace6) | `face_recognition` (InsightFace) | `facenet` |
|---------|-------------------------------|--------------------------------|-----------|
| Backend | SeetaFace6 (TenniS) | OpenCV DNN | OpenCV DNN |
| All-in-one | ✅ Detect+Landmark+Recognize | ❌ Cần nhiều node riêng | ❌ |
| Mask support | ✅ Dual-model tự động | ❌ | ❌ |
| Database | ✅ Built-in (save/load) | ❌ Cần tự implement | ❌ |
| GPU | CPU only | CPU/CUDA/TRT | CPU only |
| Accuracy (no mask) | ~97% | ~99% | ~95% |
| Speed (CPU) | ~30-50ms/face | ~20-40ms/face | ~40-60ms/face |
