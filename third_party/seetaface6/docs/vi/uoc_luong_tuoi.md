# Bộ ước lượng tuổi

## **1. Tổng quan giao diện**

Bộ ước lượng tuổi yêu cầu dữ liệu ảnh gốc và điểm đặc trưng khuôn mặt (hoặc dữ liệu khuôn mặt đã cắt), ước lượng tuổi của khuôn mặt đầu vào.

## **2. Mô tả kiểu dữ liệu**

### **2.1 struct SeetaImageData**

| Tên | Kiểu | Mô tả |
|---|---|---|
| data | uint8_t* | Dữ liệu ảnh |
| width | int32_t | Chiều rộng ảnh |
| height | int32_t | Chiều cao ảnh |
| channels | int32_t | Số kênh |

### **2.2 struct SeetaPointF**

| Tên | Kiểu | Mô tả |
|---|---|---|
| x | double | Tọa độ X điểm đặc trưng |
| y | double | Tọa độ Y điểm đặc trưng |

## 3. class AgePredictor

### 3.1 Hàm khởi tạo

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình |

### 3.2 Các hàm thành viên

#### GetCropFaceWidth / GetCropFaceHeight / GetCropFaceChannels
Lấy kích thước khuôn mặt sau cắt. Trả về `int`.

#### CropFace
Cắt khuôn mặt từ ảnh.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| points | const SeetaPointF* | | Mảng điểm đặc trưng |
| face | SeetaImageData& | | Khuôn mặt đã cắt (output) |
| Trả về | bool | | true nếu thành công |

#### PredictAge
Ước lượng tuổi từ khuôn mặt đã cắt.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| face | const SeetaImageData& | | Khuôn mặt đã cắt |
| age | int& | | Tuổi ước lượng |
| Trả về | bool | | true nếu thành công |

#### PredictAgeWithCrop
Ước lượng tuổi từ ảnh gốc với điểm đặc trưng.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| points | const SeetaPointF* | | Điểm đặc trưng |
| age | int& | | Tuổi ước lượng |
| Trả về | bool | | true nếu thành công |

#### set / get
Thiết lập/lấy thuộc tính. **PROPERTY_NUMBER_THREADS**: số luồng, mặc định 4.
