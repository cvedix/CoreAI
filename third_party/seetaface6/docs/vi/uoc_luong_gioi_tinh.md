# Bộ ước lượng giới tính

## **1. Tổng quan giao diện**

Bộ ước lượng giới tính yêu cầu dữ liệu ảnh gốc và điểm đặc trưng khuôn mặt (hoặc dữ liệu khuôn mặt đã cắt), ước lượng giới tính của khuôn mặt đầu vào.

## **2. Mô tả kiểu dữ liệu**

### **2.1 struct SeetaImageData** / **2.2 struct SeetaPointF**

Giống các module khác.

## 3. class GenderPredictor

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

#### PredictGender
Ước lượng giới tính từ khuôn mặt đã cắt.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| face | const SeetaImageData& | | Khuôn mặt đã cắt |
| gender | GENDER& | | Giới tính ước lượng |
| Trả về | bool | | true nếu thành công |

Ghi chú: GENDER có giá trị `MALE` (nam) và `FEMALE` (nữ).

#### PredictGenderWithCrop
Ước lượng giới tính từ ảnh gốc với điểm đặc trưng.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| points | const SeetaPointF* | | Điểm đặc trưng |
| gender | GENDER& | | Giới tính ước lượng |
| Trả về | bool | | true nếu thành công |

#### set / get
Thiết lập/lấy thuộc tính. **PROPERTY_NUMBER_THREADS**: số luồng, mặc định 4.
