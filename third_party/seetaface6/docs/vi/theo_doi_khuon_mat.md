# Bộ theo dõi khuôn mặt

## **1. Tổng quan giao diện**

Bộ theo dõi khuôn mặt theo dõi các khuôn mặt trong khung hình video màu hoặc xám và trả về thông tin của tất cả các khuôn mặt được theo dõi.

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

### **2.3 struct SeetaTrackingFaceInfo**

| Tên | Kiểu | Mô tả |
|---|---|---|
| pos | SeetaRect | Vị trí khuôn mặt |
| score | float | Điểm tin cậy |
| frame_no | int | Chỉ số khung hình video |
| PID | int | ID định danh theo dõi khuôn mặt |

### **2.4 struct SeetaTrackingFaceInfoArray**

| Tên | Kiểu | Mô tả |
|---|---|---|
| data | const SeetaTrackingFaceInfo* | Mảng thông tin khuôn mặt |
| size | int | Độ dài mảng |

## 3. class FaceTracker

### 3.1 Hàm khởi tạo

#### FaceTracker

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình bộ theo dõi |
| video_width | int | | Chiều rộng khung hình video |
| video_height | int | | Chiều cao khung hình video |

### 3.2 Các hàm thành viên

#### SetSingleCalculationThreads
Thiết lập số luồng tính toán.

#### Track
Theo dõi khuôn mặt trong khung hình video.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh gốc |
| Trả về | SeetaTrackingFaceInfoArray | | Mảng khuôn mặt đã theo dõi |

#### Track (với số khung hình)

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Dữ liệu ảnh gốc |
| frame_no | int | | Chỉ số khung hình |
| Trả về | SeetaTrackingFaceInfoArray | | Mảng khuôn mặt đã theo dõi |

#### SetMinFaceSize
Thiết lập kích thước mặt nhỏ nhất có thể phát hiện. Phải >= 20. Giá trị nhỏ hơn phát hiện mặt nhỏ hơn nhưng chậm hơn.

#### GetMinFaceSize
Lấy kích thước mặt nhỏ nhất hiện tại. Trả về `int32_t`.

#### SetThreshold / GetThreshold
Thiết lập/lấy ngưỡng phát hiện.

#### SetVideoStable
Bật chế độ ổn định cho kết quả theo dõi. Chỉ sử dụng khi theo dõi video liên tục.

#### GetVideoStable
Kiểm tra chế độ ổn định có bật không. Trả về `bool`.
