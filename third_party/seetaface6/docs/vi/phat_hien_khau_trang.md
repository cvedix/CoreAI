# Bộ phát hiện khẩu trang

## **1. Tổng quan giao diện**

Bộ phát hiện khẩu trang nhận dữ liệu ảnh và vị trí khuôn mặt, trả về kết quả phát hiện việc đeo khẩu trang.

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

## 3. class MaskDetector

### 3.1 Hàm khởi tạo

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình bộ phát hiện |

### 3.2 Các hàm thành viên

#### detect
Phát hiện khuôn mặt có đeo khẩu trang không.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| score | float* | nullptr | Điểm tin cậy đeo khẩu trang |
| Trả về | bool | | true nếu đeo khẩu trang |
