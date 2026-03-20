# Bộ nhận dạng khuôn mặt

## **1. Tổng quan giao diện**

Bộ nhận dạng khuôn mặt yêu cầu đầu vào là dữ liệu ảnh gốc và các điểm đặc trưng khuôn mặt (hoặc dữ liệu khuôn mặt đã được cắt), trích xuất mảng giá trị đặc trưng từ khuôn mặt đầu vào, và so sánh độ tương đồng dựa trên mảng đặc trưng đã trích xuất.

## **2. Mô tả kiểu dữ liệu**

### **2.1 struct SeetaImageData**

| Tên | Kiểu | Mô tả |
|---|---|---|
| data | uint8_t* | Dữ liệu ảnh |
| width | int32_t | Chiều rộng ảnh |
| height | int32_t | Chiều cao ảnh |
| channels | int32_t | Số kênh ảnh |

Ghi chú: Lưu trữ ảnh màu (3 kênh) hoặc ảnh xám (1 kênh). Pixel lưu liên tục theo thứ tự hàng. Ảnh màu dùng định dạng BGR888.

### **2.2 struct SeetaPointF**

| Tên | Kiểu | Mô tả |
|---|---|---|
| x | double | Tọa độ X điểm đặc trưng |
| y | double | Tọa độ Y điểm đặc trưng |

## 3. class FaceRecognizer

Bộ nhận dạng khuôn mặt.

### 3.1 Enum SeetaDevice

| Tên | Mô tả |
|---|---|
| SEETA_DEVICE_AUTO | Tự động phát hiện, ưu tiên GPU |
| SEETA_DEVICE_CPU | Sử dụng CPU |
| SEETA_DEVICE_GPU | Sử dụng GPU |

### 3.2 struct SeetaModelSetting

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| model | const char** | | Model nhận dạng |
| id | int | | GPU id |
| device | SeetaDevice | AUTO | Thiết bị tính toán |

### 3.3 Hàm khởi tạo

#### FaceRecognizer

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình bộ nhận dạng |

### 3.4 Các hàm thành viên

#### GetCropFaceWidth
Lấy chiều rộng khuôn mặt sau cắt. Trả về `int`.

#### GetCropFaceHeight
Lấy chiều cao khuôn mặt sau cắt. Trả về `int`.

#### GetCropFaceChannels
Lấy số kênh dữ liệu khuôn mặt sau cắt. Trả về `int`.

#### CropFace
Cắt khuôn mặt từ ảnh.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh gốc |
| points | const SeetaPointF* | | Mảng điểm đặc trưng |
| face | SeetaImageData& | | Khuôn mặt đã cắt (output) |
| Trả về | bool | | true nếu cắt thành công |

#### CropFaceV2 / GetCropFaceWidthV2 / GetCropFaceHeightV2 / GetCropFaceChannelsV2
Phiên bản V2 của các hàm cắt khuôn mặt (non-static).

#### GetExtractFeatureSize
Lấy độ dài mảng vector đặc trưng. Trả về `int`.

#### ExtractCroppedFace
Trích xuất vector đặc trưng từ ảnh khuôn mặt đã cắt.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| face | const SeetaImageData& | | Ảnh khuôn mặt đã cắt |
| features | float* | | Mảng đặc trưng (output) |
| Trả về | bool | | true nếu trích xuất thành công |

#### Extract
Trích xuất vector đặc trưng từ ảnh gốc sử dụng điểm đặc trưng.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh khuôn mặt gốc |
| points | const SeetaPointF* | | Mảng điểm đặc trưng |
| features | float* | | Mảng đặc trưng (output) |
| Trả về | bool | | true nếu trích xuất thành công |

#### CalculateSimilarity
So sánh hai mảng đặc trưng khuôn mặt và lấy điểm tương đồng.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| features1 | const float* | | Mảng đặc trưng 1 |
| features2 | const float* | | Mảng đặc trưng 2 |
| Trả về | float | | Giá trị tương đồng |

#### set / get
Thiết lập/lấy thuộc tính. **PROPERTY_NUMBER_THREADS**: số luồng tính toán, mặc định 4.
