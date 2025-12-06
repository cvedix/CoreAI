# Hướng dẫn Đăng ký Khuôn mặt để Nhận diện

## Tổng quan

Document này hướng dẫn cách đăng ký khuôn mặt vào database và sử dụng hệ thống nhận diện khuôn mặt với InsightFace TensorRT.

## Kiến trúc hệ thống

```
┌─────────────────────────────────────────────────────────────┐
│  Face Registration Flow                                      │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  Image → Face Detection → Face Alignment →                   │
│  Extract Embedding → Save to Database                        │
│                                                               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Face Recognition Flow                                       │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  Video Stream → Face Detection → Extract Embedding →         │
│  Compare with Database → Identify Person                     │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

## 1. Đăng ký Khuôn mặt từ Ảnh

### Bước 1: Chuẩn bị ảnh

**Yêu cầu ảnh tốt**:
- ✅ Khuôn mặt rõ ràng, đủ sáng
- ✅ Khuôn mặt nhìn thẳng, không quá nghiêng
- ✅ Chất lượng ảnh tốt (ít noise)
- ✅ Khuôn mặt chiếm ít nhất 30% ảnh

**Không nên dùng**:
- ❌ Ảnh mờ, tối
- ❌ Khuôn mặt bị che (mask, tay, ...)
- ❌ Khuôn mặt quá nhỏ trong ảnh
- ❌ Nhiều khuôn mặt trong ảnh (chỉ lấy khuôn mặt đầu tiên)

### Bước 2: Đăng ký bằng command line

```bash
cd /home/cvedix/core_ai_runtime

# Đăng ký một người
./build/bin/insightface_register_face_sample register alice.jpg "Alice"

# Đăng ký thêm người khác
./build/bin/insightface_register_face_sample register bob.jpg "Bob"
./build/bin/insightface_register_face_sample register charlie.jpg "Charlie"
```

### Bước 3: Kiểm tra database

Database được lưu trong file `./face_database.txt`. Format:

```
Alice|0.123,0.456,0.789,...
Bob|0.234,0.567,0.890,...
Charlie|0.345,0.678,0.901,...
```

Mỗi dòng chứa:
- Tên người (trước dấu `|`)
- Embedding vector 512 chiều (sau dấu `|`, phân cách bằng dấu `,`)

## 2. Nhận diện Khuôn mặt trong Video

### Sử dụng sample code

```bash
# Nhận diện trong video
./build/bin/insightface_register_face_sample recognize face.mp4

# Hoặc dùng video mặc định
./build/bin/insightface_register_face_sample recognize
```

### Kết quả

- Hệ thống sẽ hiển thị tên người được nhận diện trên màn hình
- Console sẽ log thông tin recognition:
  ```
  [Recognition] Face at (100,200) -> Alice (0.78)
  [Recognition] Face at (300,150) -> Bob (0.85)
  ```

## 3. Sử dụng trong Code

### 3.1. Đăng ký khuôn mặt programmatically

```cpp
#include "third_party/trt_insightface/models/insight_face_recognition.h"
#include <opencv2/opencv.hpp>

// Tạo database
auto db = std::make_shared<FaceDatabase>();

// Đọc ảnh
cv::Mat image = cv::imread("alice.jpg");

// Đăng ký (cần detect face trước, xem sample code)
db->register_face_from_image("alice.jpg", "Alice");
```

### 3.2. Nhận diện trong pipeline

```cpp
#include "cvedix/nodes/infers/cvedix_trt_insight_face_recognition_node.h"
#include "third_party/trt_insightface/util/algorithm_util.h"

// Load database
auto db = std::make_shared<FaceDatabase>();

// Callback để nhận diện
void recognition_callback(std::string node_name, int queue_size, 
                         std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!frame_meta) return;

    for (auto& face : frame_meta->face_targets) {
        if (!face->embeddings.empty()) {
            std::string person_name = db->identify(face->embeddings);
            std::cout << "Recognized: " << person_name << std::endl;
        }
    }
}

// Attach callback
recognizer->set_meta_handled_hooker(recognition_callback);
```

## 4. Database Management

### 4.1. Xem danh sách đã đăng ký

Database sẽ tự động list khi chạy recognition mode. Hoặc bạn có thể:

```cpp
auto db = std::make_shared<FaceDatabase>();
db->list_all();
```

### 4.2. Thay đổi ngưỡng nhận diện

Mặc định threshold là 0.6. Có thể điều chỉnh:

```cpp
db->set_threshold(0.7);  // Tăng độ chặt chẽ (ít false positive hơn)
db->set_threshold(0.5);  // Giảm độ chặt chẽ (nhận diện dễ hơn)
```

**Ngưỡng khuyến nghị**:
- **Strict** (high security): 0.70
- **Balanced**: 0.60 (mặc định)
- **Loose** (high recall): 0.50

### 4.3. Xóa người khỏi database

Xóa trực tiếp trong file `face_database.txt` hoặc tạo lại database.

### 4.4. Backup database

```bash
cp face_database.txt face_database_backup.txt
```

## 5. Best Practices

### 5.1. Đăng ký nhiều ảnh cho một người

Để tăng độ chính xác, đăng ký nhiều ảnh của cùng một người:

```cpp
// Đăng ký nhiều ảnh của Alice
db->register_face_from_image("alice_1.jpg", "Alice_photo1");
db->register_face_from_image("alice_2.jpg", "Alice_photo2");
db->register_face_from_image("alice_3.jpg", "Alice_photo3");
```

Sau đó khi nhận diện, so sánh với tất cả và lấy kết quả tốt nhất.

### 5.2. Cập nhật database định kỳ

- Xóa những người không còn cần thiết
- Cập nhật ảnh cũ bằng ảnh mới hơn
- Backup database thường xuyên

### 5.3. Tối ưu hiệu suất

- Giới hạn số lượng người trong database (nên < 1000)
- Sử dụng indexing nếu database lớn
- Cache embeddings để tăng tốc so sánh

## 6. Troubleshooting

### 6.1. Không detect được khuôn mặt khi đăng ký

**Nguyên nhân**:
- Ảnh không có khuôn mặt
- Ảnh quá tối/mờ
- Khuôn mặt quá nhỏ

**Giải pháp**:
- Kiểm tra ảnh có khuôn mặt rõ ràng không
- Tăng độ sáng ảnh
- Crop ảnh để khuôn mặt lớn hơn

### 6.2. Nhận diện sai người

**Nguyên nhân**:
- Ngưỡng threshold quá thấp
- Ảnh đăng ký không tốt
- Khuôn mặt bị che/mờ trong video

**Giải pháp**:
- Tăng threshold lên 0.7
- Đăng ký lại với ảnh tốt hơn
- Cải thiện điều kiện ánh sáng

### 6.3. Không nhận diện được người đã đăng ký

**Nguyên nhân**:
- Threshold quá cao
- Khuôn mặt thay đổi nhiều (râu, tóc, ...)
- Góc nhìn khác với ảnh đăng ký

**Giải pháp**:
- Giảm threshold xuống 0.5
- Đăng ký nhiều ảnh ở các góc độ khác nhau
- Đăng ký lại với ảnh mới hơn

### 6.4. Database file bị corrupt

**Giải pháp**:
- Khôi phục từ backup
- Tạo lại database từ đầu
- Kiểm tra format file (mỗi dòng: `name|embedding,...`)

## 7. Sample Code Đầy đủ

Xem file `samples/insightface_register_face_sample.cpp` để có implementation đầy đủ.

## 8. Advanced: Tích hợp vào Production System

### 8.1. Sử dụng SQLite/PostgreSQL

Thay vì lưu file text, có thể dùng database:

```cpp
// Pseudocode
class FaceDatabaseSQL {
    void register_face(const std::string& name, const std::vector<float>& embedding) {
        // Save to SQLite/PostgreSQL
        sqlite3_exec(db, "INSERT INTO faces (name, embedding) VALUES (?, ?)", ...);
    }
};
```

### 8.2. REST API để đăng ký

Tạo REST API endpoint:

```cpp
// POST /api/register
{
    "name": "Alice",
    "image_base64": "..."
}
```

### 8.3. Real-time Recognition với RTSP

```cpp
auto rtsp_src = std::make_shared<cvedix_rtsp_src_node>(...);
// ... pipeline với recognition
```

## 9. Tóm tắt

1. **Đăng ký**: Sử dụng ảnh tốt, chạy command `register`
2. **Nhận diện**: Load database, chạy pipeline với recognition callback
3. **Database**: Lưu trong `face_database.txt`, format `name|embedding`
4. **Threshold**: Điều chỉnh theo nhu cầu (0.5-0.7)
5. **Best practice**: Đăng ký nhiều ảnh, backup database, tối ưu số lượng

---

**Xem thêm**:
- `samples/insightface_register_face_sample.cpp` - Sample code đầy đủ
- `doc/FACE_RECOGNITION_INSIGHTFACE.md` - Tài liệu về face recognition
- `third_party/trt_insightface/` - Library implementation





