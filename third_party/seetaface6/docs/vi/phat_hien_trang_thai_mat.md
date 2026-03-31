# Bộ phát hiện trạng thái mắt

## **1. Tổng quan giao diện**

Bộ phát hiện trạng thái mắt yêu cầu dữ liệu ảnh gốc và điểm đặc trưng khuôn mặt, trả về trạng thái mắt trái và mắt phải.

## **2. Mô tả kiểu dữ liệu**

### **2.1 struct SeetaImageData** / **2.2 struct SeetaPointF**

Giống các module khác.

## 3. class EyeStateDetector

### 3.1 Hàm khởi tạo

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình bộ phát hiện |

### 3.2 Các hàm thành viên

#### Detect
Phát hiện trạng thái mắt từ ảnh gốc và điểm đặc trưng.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| points | const SeetaPointF* | | Mảng điểm đặc trưng |
| leftState | EYE_STATE | | Trạng thái mắt trái |
| rightState | EYE_STATE | | Trạng thái mắt phải |

Giá trị EYE_STATE:
- `EYE_CLOSE` — Mắt nhắm
- `EYE_OPEN` — Mắt mở
- `EYE_RANDOM` — Vùng không phải mắt
- `EYE_UNKNOWN` — Trạng thái không xác định

#### set / get
Thiết lập/lấy thuộc tính. **PROPERTY_NUMBER_THREADS**: số luồng, mặc định 4.
