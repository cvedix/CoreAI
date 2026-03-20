# Bộ phát hiện khuôn mặt

## **1. Tổng quan giao diện**

Bộ phát hiện khuôn mặt thực hiện phát hiện khuôn mặt trên ảnh màu hoặc ảnh xám đầu vào và trả về vị trí của tất cả các khuôn mặt được phát hiện.

## **2. Mô tả kiểu dữ liệu**

### **2.1 struct SeetaImageData**

| Tên | Kiểu | Mô tả |
|---|---|---|
| data | uint8_t* | Dữ liệu ảnh |
| width | int32_t | Chiều rộng ảnh |
| height | int32_t | Chiều cao ảnh |
| channels | int32_t | Số kênh ảnh |

Ghi chú: Lưu trữ ảnh màu (3 kênh) hoặc ảnh xám (1 kênh). Pixel được lưu liên tục theo thứ tự hàng. Ảnh màu sử dụng định dạng BGR888, ảnh xám sử dụng giá trị byte đơn.

### **2.2 struct SeetaRect**

| Tên | Kiểu | Mô tả |
|---|---|---|
| x | int32_t | Tọa độ X góc trên bên trái vùng mặt |
| y | int32_t | Tọa độ Y góc trên bên trái vùng mặt |
| width | int32_t | Chiều rộng vùng mặt |
| height | int32_t | Chiều cao vùng mặt |

### **2.3 struct SeetaFaceInfo**

| Tên | Kiểu | Mô tả |
|---|---|---|
| pos | SeetaRect | Vị trí khuôn mặt |
| score | float | Điểm tin cậy khuôn mặt |

### **2.4 struct SeetaFaceInfoArray**

| Tên | Kiểu | Mô tả |
|---|---|---|
| data | const SeetaFaceInfo* | Mảng thông tin khuôn mặt |
| size | int | Độ dài mảng |

## 3. class FaceDetector

Bộ phát hiện khuôn mặt.

### 3.1 Enum SeetaDevice

Thiết bị tính toán để chạy model.

| Tên | Mô tả |
|---|---|
| SEETA_DEVICE_AUTO | Tự động phát hiện, ưu tiên GPU |
| SEETA_DEVICE_CPU | Sử dụng CPU |
| SEETA_DEVICE_GPU | Sử dụng GPU |

### 3.2 struct SeetaModelSetting

Tham số cần truyền vào để khởi tạo bộ phát hiện khuôn mặt.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| model | const char** | | Model phát hiện |
| id | int | | GPU id |
| device | SeetaDevice | AUTO | Thiết bị tính toán (CPU hoặc GPU) |

### 3.3 Hàm khởi tạo

#### FaceDetector

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình bộ phát hiện |

### 3.4 Các hàm thành viên

#### detect

Phát hiện khuôn mặt trong ảnh màu.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh đầu vào |
| Trả về | SeetaFaceInfoArray | | Mảng thông tin khuôn mặt |

#### set

Thiết lập thuộc tính bộ phát hiện:
- **PROPERTY_MIN_FACE_SIZE**: Kích thước mặt nhỏ nhất có thể phát hiện. Giá trị càng nhỏ, phát hiện được mặt càng nhỏ nhưng tốc độ chậm hơn. Mặc định: 20.
- **PROPERTY_THRESHOLD**: Ngưỡng lọc phát hiện. Mặc định: 0.90.
- **PROPERTY_MAX_IMAGE_WIDTH** / **PROPERTY_MAX_IMAGE_HEIGHT**: Kích thước ảnh đầu vào tối đa.
- **PROPERTY_NUMBER_THREADS**: Số luồng tính toán. Mặc định: 4.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| property | Property | | Loại thuộc tính |
| value | double | | Giá trị thuộc tính |
| Trả về | void | | |

#### get

Lấy giá trị thuộc tính của bộ phát hiện.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| property | Property | | Loại thuộc tính |
| Trả về | double | | Giá trị thuộc tính tương ứng |
