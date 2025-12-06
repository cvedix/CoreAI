# Face Recognition trên hình ảnh - Hướng dẫn sử dụng

## Tổng quan

Sample `insightface_register_face_trt_sample` giờ đã hỗ trợ nhận diện khuôn mặt trên cả **hình ảnh** và **video**. Program tự động detect loại input dựa trên extension file.

## Các định dạng ảnh được hỗ trợ

- `.jpg`, `.jpeg`
- `.png`
- `.bmp`
- `.tiff`
- `.webp`

## Cách sử dụng

### 1. Đăng ký khuôn mặt (bắt buộc trước)

```bash
cd /home/cvedix/core_ai_runtime

# Đăng ký khuôn mặt từ ảnh
./build/bin/insightface_register_face_trt_sample register \
  build/bin/cvedix_data/test_images/face_recognition/NAME_1.png "NAME_1"
```

### 2. Nhận diện trên hình ảnh

```bash
# Nhận diện từ ảnh
./build/bin/insightface_register_face_trt_sample recognize \
  build/bin/cvedix_data/test_images/face_recognition/NAME_1.png
```

**Output**:
- Console log chi tiết về các khuôn mặt được detect và nhận diện
- File `recognition_result.jpg` với bounding box và tên người được vẽ lên ảnh

### 3. Nhận diện trên video (như trước)

```bash
# Nhận diện từ video
./build/bin/insightface_register_face_trt_sample recognize \
  cvedix_data/test_video/face.mp4
```

## Kết quả mẫu

### Nhận diện trên ảnh NAME_1.png:

```
================================================================
          Face Recognition Mode
================================================================

Input type: Image

[DB] Registered faces (1):
  - NAME_1

Configuration:
  Input: /home/cvedix/core_ai_runtime/./build/bin/cvedix_data/test_images/face_recognition/NAME_1.png
  Type: Image
  Database: 1 registered faces
  Threshold: 0.6


[Image Recognition] Processing: .../NAME_1.png
[Image Recognition] Image size: 598x718
[Image Recognition] Detected 1 face(s)
[Image Recognition] Recognition engine loaded

[Results]
  Face 1: (205,142) 300x426 -> NAME_1 (1.00) (detection score: 0.894377)

[Output] Result saved to: recognition_result.jpg
```

### Output file

**File**: `recognition_result.jpg` (được tạo trong thư mục hiện tại)

Nội dung:
- Hình gốc với bounding box màu xanh lá vẽ quanh khuôn mặt
- Tên người được nhận diện hiển thị phía trên bounding box
- Format: `TÊN_NGƯỜI (SIMILARITY_SCORE)`

Ví dụ: `NAME_1 (1.00)` có nghĩa là nhận diện NAME_1 với độ tương đồng 100%

## Chi tiết output

### Console output

Mỗi khuôn mặt được detect sẽ hiển thị thông tin:
- **Position**: `(x, y)` - tọa độ góc trên bên trái
- **Size**: `width x height` - kích thước bounding box
- **Identity**: Tên người nhận diện được và similarity score
- **Detection score**: Confidence của face detector (0.0-1.0)

Format: `Face N: (x,y) WxH -> IDENTITY (detection score: X.XXX)`

### Similarity score

- **> 0.8**: Rất chắc chắn (Very confident match)
- **0.6 - 0.8**: Khá chắc chắn (Confident match) - đây là threshold mặc định
- **< 0.6**: Unknown - không đủ tin cậy để xác định

## Workflow hoàn chỉnh

### Bước 1: Chuẩn bị ảnh đăng ký

Đảm bảo ảnh đăng ký có:
- Ít nhất 1 khuôn mặt rõ ràng
- Khuôn mặt không bị che khuất quá nhiều
- Độ phân giải đủ tốt (khuyến nghị > 200x200 pixels cho vùng khuôn mặt)

### Bước 2: Đăng ký nhiều người

```bash
# Đăng ký Alice
./build/bin/insightface_register_face_trt_sample register alice.jpg "Alice"

# Đăng ký Bob
./build/bin/insightface_register_face_trt_sample register bob.jpg "Bob"

# Đăng ký Charlie
./build/bin/insightface_register_face_trt_sample register charlie.jpg "Charlie"
```

### Bước 3: Nhận diện trên ảnh test

```bash
# Test trên ảnh có nhiều người
./build/bin/insightface_register_face_trt_sample recognize group_photo.jpg

# Kiểm tra kết quả
ls -lh recognition_result.jpg
```

## Các trường hợp đặc biệt

### Không detect được khuôn mặt

```
[Result] No faces detected in image
```

**Nguyên nhân**:
- Ảnh không có khuôn mặt
- Khuôn mặt quá nhỏ
- Khuôn mặt bị che khuất quá nhiều
- Góc chụp quá nghiêng

**Giải pháp**:
- Sử dụng ảnh chất lượng tốt hơn
- Đảm bảo khuôn mặt rõ ràng và đủ lớn

### Database rỗng

```
[Error] Database is empty! Please register faces first.
```

**Giải pháp**: Chạy mode `register` trước để đăng ký ít nhất 1 khuôn mặt.

### Unknown face

```
Face 1: (100,150) 200x300 -> Unknown (detection score: 0.892)
```

**Nguyên nhân**:
- Khuôn mặt không có trong database
- Similarity score < threshold (0.6)

**Giải pháp**:
- Đăng ký khuôn mặt này vào database
- Hoặc điều chỉnh threshold (hiện tại hardcoded trong code)

## Performance

### Trên ảnh
- **Fast**: Xử lý ngay lập tức, không cần pipeline
- **Memory efficient**: Chỉ load models khi cần
- **Accurate**: Sử dụng cùng models với video mode

### So sánh với video mode
- **Image mode**: 
  - Simple, direct processing
  - Save result to file
  - No real-time display needed
  
- **Video mode**:
  - Pipeline-based processing
  - Real-time display with GStreamer
  - Continuous frame processing

## Troubleshooting

### Lỗi "Model not found"
```bash
# Kiểm tra models tồn tại
ls -lh build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx
ls -lh build/bin/cvedix_data/models/trt/face/w600k_mbf_fp16_trt10.9.engine
```

### Lỗi "Cannot read image"
```bash
# Kiểm tra file tồn tại và có quyền đọc
ls -lh <image_path>
file <image_path>  # Kiểm tra định dạng file
```

### Output image không được tạo
- Kiểm tra quyền ghi trong thư mục hiện tại
- Đảm bảo có ít nhất 1 face được detect

## Notes

1. **Database path**: Mặc định là `./face_database.txt` trong thư mục hiện tại
2. **Output image**: Luôn là `recognition_result.jpg` (overwrite nếu đã tồn tại)
3. **Models**: Tự động tìm trong `build/bin/cvedix_data/` hoặc `cvedix_data/`
4. **Thread-safe**: Image mode không sử dụng global state nguy hiểm

## Advanced Usage

### Batch processing nhiều ảnh

```bash
#!/bin/bash
# Script để nhận diện nhiều ảnh

for img in *.jpg; do
    echo "Processing $img..."
    ./build/bin/insightface_register_face_trt_sample recognize "$img"
    mv recognition_result.jpg "result_${img}"
done
```

### Integration với script

```bash
#!/bin/bash
# Check if face is recognized

result=$(./build/bin/insightface_register_face_trt_sample recognize test.jpg | grep "Unknown")

if [ -z "$result" ]; then
    echo "Face recognized successfully!"
else
    echo "Unknown face detected"
fi
```




