# Sửa lỗi OpenCV trong insightface_register_face_trt_sample

## Lỗi gốc

```
terminate called after throwing an instance of 'cv::Exception'
  what():  OpenCV(4.10.0) /home/cvedix/opencv-4.10.0/modules/dnn/src/net_impl.cpp:279: error: (-204:Requested object was not found) Layer with requested id=-1 not found in function 'getLayerData'
```

## Nguyên nhân

1. **Cách truy cập dữ liệu faces sai**: Code đang dùng pointer access trực tiếp (`float* face_data = (float*)faces.data`) thay vì dùng `.at<float>(row, col)` như OpenCV yêu cầu.

2. **Thiếu error handling**: Không có try-catch khi tạo `FaceDetectorYN`, nên lỗi không được bắt và xử lý đúng cách.

3. **Không validate model file**: Không kiểm tra model file trước khi sử dụng.

## Các sửa đổi đã thực hiện

### 1. Sửa cách truy cập dữ liệu faces

**Trước** (sai):
```cpp
float* face_data = (float*)faces.data;
int x = (int)(face_data[0]);
int y = (int)(face_data[1]);
int w = (int)(face_data[2]);
int h = (int)(face_data[3]);
float score = face_data[14];
```

**Sau** (đúng):
```cpp
// FaceDetectorYN output format: each row is a face with 15 values:
// [x, y, w, h, re_x, re_y, le_x, le_y, nt_x, nt_y, rcm_x, rcm_y, lcm_x, lcm_y, score]
int face_idx = 0;
float x = faces.at<float>(face_idx, 0);
float y = faces.at<float>(face_idx, 1);
float w = faces.at<float>(face_idx, 2);
float h = faces.at<float>(face_idx, 3);
float score = faces.at<float>(face_idx, 14);
```

### 2. Thêm error handling

- Thêm try-catch khi tạo `FaceDetectorYN`
- Thêm try-catch khi gọi `detect()`
- Validate model file trước khi sử dụng

### 3. Thêm validation

- Kiểm tra model file tồn tại và là file hợp lệ
- Kiểm tra file size > 0
- Validate bounding box coordinates

### 4. Cải thiện logging

- Log model file path và size
- Log error messages chi tiết hơn
- Log từng bước trong quá trình detect

## Build và test lại

```bash
cd /home/cvedix/core_ai_runtime/build
make insightface_register_face_trt_sample

# Test đăng ký
./bin/insightface_register_face_trt_sample register \
  ../cvedix_data/test_images/face_recognition/NAME_1.png "NAME_1"
```

## Lưu ý

1. **Model file**: Đảm bảo model file `face_detection_yunet_2022mar.onnx` tồn tại và đúng format
2. **OpenCV version**: Yêu cầu OpenCV 4.5+ với module `objdetect` được compile
3. **Image format**: Ảnh input phải là BGR format (như `cv::imread()` trả về)

## Format output của FaceDetectorYN

Mỗi row trong `faces` Mat chứa 15 giá trị:
- Index 0-3: x, y, w, h (bounding box)
- Index 4-13: landmarks (5 điểm, mỗi điểm 2 giá trị x, y)
  - 4-5: right eye
  - 6-7: left eye
  - 8-9: nose tip
  - 10-11: right corner of mouth
  - 12-13: left corner of mouth
- Index 14: confidence score

