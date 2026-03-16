# Bộ đánh giá chất lượng

## **1. Tổng quan giao diện**

Các bộ đánh giá chất lượng đánh giá chất lượng ảnh khuôn mặt từ nhiều khía cạnh, trả về mức chất lượng và điểm số giúp xác định ảnh khuôn mặt có phù hợp để nhận dạng hay không.

## **2. Mô tả kiểu dữ liệu**

### **2.1 struct QualityResult**

| Tên | Kiểu | Mô tả |
|---|---|---|
| level | QualityLevel | Mức chất lượng: LOW, MEDIUM, HIGH |
| score | float | Điểm chất lượng |

## 3. class QualityOfBrightness

Bộ đánh giá độ sáng khuôn mặt (không dùng deep learning).

### 3.1 Hàm khởi tạo

Mặc định, hoặc với tham số `(float v0, float v1, float v2, float v3)`.

Phân loại: `[0, v0)` và `[v3, ~)` → LOW; `[v0, v1)` và `[v2, v3)` → MEDIUM; `[v1, v2)` → HIGH.

### 3.2 Hàm thành viên

#### check

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| points | const SeetaPointF* | | Mảng 5 điểm đặc trưng |
| N | const int32_t | | Số điểm đặc trưng |
| Trả về | QualityResult | | Kết quả độ sáng |

## 4. class QualityOfClarity

Bộ đánh giá độ rõ nét khuôn mặt (không dùng deep learning).

### 4.1 Hàm khởi tạo

Mặc định, hoặc với `(float low, float high)`.

Phân loại: `[0, low)` → LOW; `[low, high)` → MEDIUM; `[high, ~)` → HIGH.

### 4.2 check
Cùng chữ ký như QualityOfBrightness. Trả về đánh giá độ rõ nét.

## 5. class QualityOfLBN

Bộ đánh giá độ rõ nét khuôn mặt (dùng deep learning, cần model).

### 5.1 Hàm khởi tạo

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình model |

### 5.2 Hàm thành viên

#### Detect

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| points | const SeetaPointF* | | Mảng 68 điểm đặc trưng |
| light | int* | | Kết quả độ sáng (không khuyến nghị) |
| blur | int* | | Kết quả mờ (0=rõ, 1=mờ) |
| noise | int* | | Kết quả nhiễu (không khuyến nghị) |

#### set / get
Thuộc tính:
- **PROPERTY_NUMBER_THREADS**: Số luồng, mặc định 4.
- **PROPERTY_ARM_CPU_MODE**: Chế độ CPU mobile. 0=nhân lớn, 1=nhân nhỏ, 2=cân bằng (mặc định).
- **PROPERTY_BLUR_THRESH**: Ngưỡng mờ, mặc định 0.80.

## 6. class QualityOfPose

Bộ đánh giá tư thế khuôn mặt (không dùng deep learning).

### 6.1 Hàm khởi tạo
Mặc định (không tham số).

### 6.2 check
Cùng chữ ký như QualityOfBrightness. Trả về chất lượng tư thế.

## 7. class QualityOfPoseEx

Bộ đánh giá tư thế khuôn mặt (dùng deep learning).

### 7.1 Hàm khởi tạo

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Cấu hình model |

### 7.2 Hàm thành viên

#### check (cơ bản)
Cùng chữ ký như QualityOfBrightness.

#### check (với góc cụ thể)
Trả về góc tư thế cụ thể.

| Tham số | Kiểu | Mặc định | Mô tả |
|---|---|---|---|
| image | const SeetaImageData& | | Ảnh gốc |
| face | const SeetaRect& | | Vị trí khuôn mặt |
| points | const SeetaPointF* | | 5 điểm đặc trưng |
| N | const int32_t | | Số điểm |
| yaw | float& | | Góc yaw |
| pitch | float& | | Góc pitch |
| roll | float& | | Góc roll |
| Trả về | bool | | true nếu thành công |

#### set / get
Thuộc tính: `YAW_HIGH_THRESHOLD`, `YAW_LOW_THRESHOLD`, `PITCH_HIGH_THRESHOLD`, `PITCH_LOW_THRESHOLD`, `ROLL_HIGH_THRESHOLD`, `ROLL_LOW_THRESHOLD`.

## 8. class QualityOfResolution

Bộ đánh giá kích thước khuôn mặt (không dùng deep learning).

### 8.1 Hàm khởi tạo
Mặc định, hoặc với `(float low, float high)`.

### 8.2 check
Cùng chữ ký. Trả về đánh giá độ phân giải.

## 9. class QualityOfIntegrity

Bộ đánh giá tính toàn vẹn khuôn mặt — đánh giá mức độ khuôn mặt gần mép ảnh.

### 9.1 Hàm khởi tạo
Mặc định, hoặc với `(float low, float high)` để kiểm soát mức chấp nhận gần mép.

### 9.2 check
Cùng chữ ký. Trả về đánh giá tính toàn vẹn.
