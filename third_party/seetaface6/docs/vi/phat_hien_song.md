# Bộ phát hiện sống im lặng (Chống giả mạo)

## **1. Tổng quan giao diện**

Bộ phát hiện sống im lặng nhận đầu vào là dữ liệu ảnh, vị trí khuôn mặt và điểm đặc trưng, xác định xem khuôn mặt có phải là người thật hay không, và trả về trạng thái sống.

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

## 3. class FaceAntiSpoofing

### 3.1 Hàm khởi tạo

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình bộ phát hiện |

Ghi chú: Có thể truyền một model (model cục bộ — nhanh hơn nhưng kém chính xác) hoặc hai model (cục bộ + toàn cục — chậm hơn nhưng chính xác hơn, thứ tự không được đảo).

### 3.2 Các hàm thành viên

#### Predict
Xác định sống từ một ảnh đơn.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| points | const SeetaPointF* | | Mảng điểm đặc trưng |
| Trả về | Status | | Trạng thái sống |

Giá trị Status: `REAL` (người thật), `SPOOF` (giả mạo), `FUZZY` (không xác định được do chất lượng ảnh), `DETECTING` (đang phát hiện — chỉ trong chế độ PredictVideo).

#### PredictVideo
Xác định sống từ chuỗi video liên tục.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| points | const SeetaPointF* | | Mảng điểm đặc trưng |
| Trả về | Status | | Trạng thái sống |

#### ResetVideo
Reset trạng thái phát hiện sống cho phiên PredictVideo mới.

#### GetPreFrameScore
Lấy điểm nội bộ.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| clarity | float* | | Điểm độ rõ nét |
| reality | float* | | Điểm sống |

#### SetVideoFrameCount / GetVideoFrameCount
Thiết lập/lấy số khung hình video cần thiết trước khi trả kết quả.

#### SetThreshold

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| clarity | float | | Ngưỡng độ rõ nét (mặc định: 0.3) |
| reality | float | | Ngưỡng sống (mặc định: 0.8) |

#### set / get
Thiết lập/lấy thuộc tính. **PROPERTY_NUMBER_THREADS**: số luồng, mặc định 4.
