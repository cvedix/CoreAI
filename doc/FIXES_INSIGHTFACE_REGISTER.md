# Các lỗi đã sửa trong insightface_register_face_trt_sample

## Các vấn đề đã khắc phục

### 1. ✅ Path Resolution - Tự động tìm project root

**Vấn đề**: Khi chạy từ `build/bin`, các đường dẫn relative (như `./cvedix_data/...`) sẽ không đúng.

**Giải pháp**: 
- Thêm function `get_project_root()` để tự động tìm project root
- Thêm function `resolve_path()` để resolve paths đúng từ bất kỳ đâu
- Tự động detect nếu đang chạy từ `build/bin` và điều chỉnh paths

**Code đã thêm**:
```cpp
static std::string get_project_root(const std::string& executable_path);
static std::string resolve_path(const std::string& executable_path, const std::string& relative_path);
```

### 2. ✅ Unicode Characters - Loại bỏ ký tự đặc biệt

**Vấn đề**: Unicode box drawing characters (`╔`, `║`, `╚`, `✓`, `✗`, `→`) hiển thị sai trên một số terminal.

**Giải pháp**: Thay thế bằng ASCII characters đơn giản:
- `╔══════╗` → `========`
- `✓` → removed
- `✗` → removed  
- `→` → `->`

### 3. ✅ Error Handling - Cải thiện thông báo lỗi

**Vấn đề**: Lỗi không rõ ràng khi không tìm thấy files.

**Giải pháp**:
- Kiểm tra và báo lỗi rõ ràng nếu model files không tồn tại
- Hiển thị danh sách paths đã thử
- Thông báo khi database rỗng

### 4. ✅ Path Resolution trong FaceDatabase

**Vấn đề**: Database và engine paths cố định, không linh hoạt.

**Giải pháp**:
- Constructor nhận `executable_path` để resolve paths
- Tự động thử nhiều paths cho engine file
- Log paths đã thử để debug

## Cách sử dụng sau khi sửa

### Chạy từ project root (khuyến nghị):
```bash
cd /home/cvedix/core_ai_runtime
./build/bin/insightface_register_face_trt_sample recognize cvedix_data/test_video/face.mp4
```

### Chạy từ build/bin:
```bash
cd /home/cvedix/core_ai_runtime/build/bin
./insightface_register_face_trt_sample recognize ../cvedix_data/test_video/face.mp4
# Hoặc
./insightface_register_face_trt_sample recognize ../../cvedix_data/test_video/face.mp4
```

Code sẽ tự động tìm project root và resolve paths đúng.

## Lưu ý

1. **Database location**: Database file (`face_database.txt`) sẽ được tạo trong project root directory
2. **Model paths**: Code tự động tìm models ở các vị trí khả dĩ
3. **Error messages**: Nếu có lỗi, sẽ hiển thị đầy đủ thông tin để debug

## Build lại

Sau khi sửa, cần build lại:
```bash
cd build
make insightface_register_face_trt_sample
```




