# Bộ phát hiện điểm đặc trưng khuôn mặt

## **1. Tổng quan giao diện**

Bộ phát hiện điểm đặc trưng yêu cầu đầu vào là dữ liệu ảnh gốc và vị trí khuôn mặt, trả về tọa độ của 5 hoặc nhiều hơn điểm đặc trưng (số lượng phụ thuộc vào model được tải).

## **2. Mô tả kiểu dữ liệu**

### **2.1 struct SeetaImageData**

| Tên | Kiểu | Mô tả |
|---|---|---|
| data | uint8_t* | Dữ liệu ảnh |
| width | int32_t | Chiều rộng ảnh |
| height | int32_t | Chiều cao ảnh |
| channels | int32_t | Số kênh |

### **2.2 struct SeetaRect**

| Tên | Kiểu | Mô tả |
|---|---|---|
| x | int32_t | Tọa độ X góc trên trái |
| y | int32_t | Tọa độ Y góc trên trái |
| width | int32_t | Chiều rộng vùng mặt |
| height | int32_t | Chiều cao vùng mặt |

### **2.3 struct SeetaPointF**

| Tên | Kiểu | Mô tả |
|---|---|---|
| x | double | Tọa độ X điểm đặc trưng |
| y | double | Tọa độ Y điểm đặc trưng |

## 3. class FaceLandmarker

### 3.1 Hàm khởi tạo

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình bộ phát hiện |

### 3.2 Các hàm thành viên

#### number
Lấy số lượng điểm đặc trưng của model hiện tại. Trả về `int`.

#### mark
Lấy các điểm đặc trưng khuôn mặt.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| points | SeetaPointF* | | Mảng điểm đặc trưng (cần cấp phát trước với độ dài từ `number()`) |
| Trả về | void | | |

#### mark (với thông tin che khuất)
Lấy điểm đặc trưng và thông tin che khuất.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| points | SeetaPointF* | | Mảng điểm đặc trưng |
| mask | int32_t* | | Mảng che khuất (1=bị che, 0=không bị che) |
| Trả về | void | | |

#### mark (trả về vector)

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| Trả về | std::vector\<SeetaPointF\> | | Mảng điểm đặc trưng |

#### mark_v2
Lấy điểm đặc trưng với thông tin che khuất dạng PointWithMask vector.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| Trả về | std::vector\<PointWithMask\> | | Điểm đặc trưng với thông tin che khuất |
