# Quick Start - Đăng ký Khuôn mặt

## ⚡ Bắt đầu nhanh

### 1. Đăng ký khuôn mặt từ ảnh

```bash
cd /home/cvedix/core_ai_runtime

# Đăng ký một người
./build/bin/insightface_register_face_sample register alice.jpg "Alice"
./build/bin/insightface_register_face_sample register bob.jpg "Bob"
```

### 2. Nhận diện trong video

```bash
# Nhận diện
./build/bin/insightface_register_face_sample recognize face.mp4
```

## 📋 Yêu cầu ảnh đăng ký

- ✅ Khuôn mặt rõ ràng, đủ sáng
- ✅ Nhìn thẳng, không quá nghiêng  
- ✅ Chất lượng tốt
- ✅ Khuôn mặt chiếm ít nhất 30% ảnh

## 📁 Database

- **File**: `./face_database.txt`
- **Format**: `name|embedding1,embedding2,embedding3,...`
- **Backup**: `cp face_database.txt face_database_backup.txt`

## 🎯 Sử dụng trong Code

### Đăng ký programmatically

```cpp
#include "samples/insightface_register_face_sample.cpp"
// Sử dụng FaceDatabase class từ sample
```

### Nhận diện trong pipeline

```cpp
auto db = std::make_shared<FaceDatabase>();
recognizer->set_meta_handled_hooker([db](...) {
    // Identify faces
    std::string name = db->identify(face->embeddings);
});
```

## ⚙️ Điều chỉnh ngưỡng

```cpp
db->set_threshold(0.7);  // Strict
db->set_threshold(0.6);  // Balanced (default)
db->set_threshold(0.5);  // Loose
```

## 📚 Tài liệu đầy đủ

Xem `doc/FACE_REGISTRATION_GUIDE.md` để biết chi tiết.

## 🔧 Build Sample

```bash
cd build
cmake .. -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON
make insightface_register_face_sample
```

---

**Lưu ý**: Sample code cần được build và test trước khi sử dụng.





