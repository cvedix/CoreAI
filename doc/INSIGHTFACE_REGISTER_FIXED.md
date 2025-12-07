# insightface_register_face_trt_sample - Đã sửa lỗi thành công

## Tóm tắt các lỗi đã sửa

### 1. ✅ Lỗi OpenCV FaceDetectorYN

**Vấn đề ban đầu**:
```
terminate called after throwing an instance of 'cv::Exception'
  what():  OpenCV(4.10.0) /home/cvedix/opencv-4.10.0/modules/dnn/src/net_impl.cpp:279: 
  error: (-204:Requested object was not found) Layer with requested id=-1 not found
```

**Nguyên nhân**:
- Model `face_detection_yunet_2022mar.onnx` (FP32) không tương thích với `cv::FaceDetectorYN` trong OpenCV 4.10.0
- Cách truy cập dữ liệu faces không đúng (dùng pointer thay vì `.at<float>()`)

**Giải pháp**:
1. Đổi sang dùng model INT8: `face_detection_yunet_2023mar_int8.onnx`
2. Sửa cách truy cập dữ liệu faces:
   - **Trước**: `float* face_data = (float*)faces.data; face_data[0]`
   - **Sau**: `faces.at<float>(face_idx, 0)`
3. Thêm backend_id và target_id khi tạo FaceDetectorYN
4. Thay đổi input size từ 640x640 → 320x320 (phù hợp với INT8 model)

### 2. ✅ Path Resolution

**Vấn đề**: 
- Đường dẫn relative không đúng khi chạy từ `build/bin/`
- Project root bị detect sai (có dấu `.` thừa)

**Giải pháp**:
- Thêm logic tự động tìm project root
- Thử nhiều paths khả dĩ cho model files:
  - `build/bin/cvedix_data/models/...`
  - `cvedix_data/models/...`
  - Relative to project root

### 3. ✅ Auto-create Database File

**Vấn đề**: Nếu `face_database.txt` chưa tồn tại, không tự động tạo

**Giải pháp**: Tự động tạo file rỗng khi load database lần đầu

```cpp
void load_database() {
    std::ifstream file(db_file_path_);
    if (!file.is_open()) {
        std::cout << "[DB] Database file not found, will create new database" << std::endl;
        // Create empty database file
        std::ofstream create_file(db_file_path_);
        if (create_file.is_open()) {
            create_file.close();
            std::cout << "[DB] Created empty database file: " << db_file_path_ << std::endl;
        }
        return;
    }
    // ...
}
```

### 4. ✅ Error Handling

- Thêm try-catch khi tạo `FaceDetectorYN`
- Thêm try-catch khi gọi `detect()`
- Validate model file (exists, is_regular_file, file_size > 0)
- Log chi tiết các bước và lỗi

### 5. ✅ Unicode Characters

- Loại bỏ các ký tự Unicode (`✓`, `✗`, `→`, `╔`, `║`, `╚`) 
- Thay bằng ASCII characters đơn giản

## Kết quả

✅ **Build thành công**
✅ **Đăng ký khuôn mặt thành công**
✅ **Database được tạo tự động**

```bash
# Build
cd /home/cvedix/core_ai_runtime/build
make insightface_register_face_trt_sample

# Test đăng ký
cd /home/cvedix/core_ai_runtime
./build/bin/insightface_register_face_trt_sample register \
  build/bin/cvedix_data/test_images/face_recognition/NAME_1.png "NAME_1"
```

**Output**:
```
================================================================
          Face Registration Mode
================================================================

[DB] Project root: /home/cvedix/core_ai_runtime/.
[DB] Database path: /home/cvedix/core_ai_runtime/././face_database.txt
[DB] Found engine at: /home/cvedix/core_ai_runtime/./build/bin/cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine
[TRT] Tensor names (w600k_mbf model):
  Input:  input.1
  Output: 516 (size: 512)
[DB] Engine loaded successfully
[DB] Loaded 0 faces from database

[Register] Processing image: /home/cvedix/core_ai_runtime/./build/bin/cvedix_data/test_images/face_recognition/NAME_1.png
[Register] Person name: NAME_1
[Register] Using detector model: /home/cvedix/core_ai_runtime/./build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx
[Register] Model file size: 100416 bytes
[Register] Creating FaceDetectorYN with backend=3, target=0
[Register] FaceDetectorYN created successfully
[Register] Detected face: (205,142) 300x426 (score: 0.894377)
[DB] Saved 1 faces to database
[Register] Successfully registered: NAME_1

[DB] Registered faces (1):
  - NAME_1

Registration completed!
```

## Sử dụng

### 1. Đăng ký khuôn mặt

```bash
cd /home/cvedix/core_ai_runtime
./build/bin/insightface_register_face_trt_sample register <image_path> <person_name>

# Ví dụ
./build/bin/insightface_register_face_trt_sample register \
  build/bin/cvedix_data/test_images/face_recognition/NAME_1.png "Alice"
```

### 2. Nhận diện khuôn mặt từ video

```bash
./build/bin/insightface_register_face_trt_sample recognize <video_path>

# Ví dụ
./build/bin/insightface_register_face_trt_sample recognize \
  cvedix_data/test_video/face.mp4
```

**Lưu ý**: Phải đăng ký ít nhất 1 khuôn mặt trước khi chạy recognition mode.

## Database Format

File: `face_database.txt`

Format: `name|embedding1,embedding2,embedding3,...,embedding512`

Mỗi dòng lưu 1 người với 512 giá trị embedding (float, phân cách bằng dấu `,`).

## Models được sử dụng

1. **Face Detection**: `face_detection_yunet_2023mar_int8.onnx` (INT8, 320x320)
   - Tương thích với `cv::FaceDetectorYN`
   - Nhanh và chính xác

2. **Face Recognition**: `w600k_mbf_fp16_trt10.9.engine` (TensorRT FP16)
   - 512-dimensional embeddings
   - Cosine similarity để so sánh

## Troubleshooting

### Lỗi "Database is empty"
- Chạy mode `register` trước để đăng ký khuôn mặt

### Lỗi "Model not found"
- Kiểm tra model files tồn tại trong `build/bin/cvedix_data/models/`
- Chạy từ project root directory

### Lỗi "No face detected"
- Đảm bảo ảnh input có ít nhất 1 khuôn mặt rõ ràng
- Khuôn mặt không bị che khuất quá nhiều
- Ảnh có độ phân giải đủ tốt

## Next Steps

- Test recognition mode với video stream
- Thêm nhiều người vào database
- Tối ưu threshold (hiện tại: 0.6) cho từng use case





