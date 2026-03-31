# Silent Liveness Detector (Anti-Spoofing)

## **1. Interface Overview**

The silent liveness detector takes image data, face position, and facial landmarks as input, determines if the face is from a live person, and returns the liveness status.

## **2. Type Descriptions**

### **2.1 struct SeetaImageData**

| Name | Type | Description |
|---|---|---|
| data | uint8_t* | Image data |
| width | int32_t | Image width |
| height | int32_t | Image height |
| channels | int32_t | Number of channels |

### **2.2 struct SeetaRect**

| Name | Type | Description |
|---|---|---|
| x | int32_t | X of top-left corner |
| y | int32_t | Y of top-left corner |
| width | int32_t | Face region width |
| height | int32_t | Face region height |

### **2.3 struct SeetaPointF**

| Name | Type | Description |
|---|---|---|
| x | double | X-coordinate of landmark |
| y | double | Y-coordinate of landmark |

## 3. class FaceAntiSpoofing

### 3.1 Constructor

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Detector configuration |

Note: Can accept one model file (local liveness model — faster but less accurate) or two model files (local + global liveness model — slower but more accurate, order matters).

### 3.2 Member Functions

#### Predict
Determine liveness from a single image.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| face | const SeetaRect& | | Face position |
| points | const SeetaPointF* | | Landmark array |
| Return | Status | | Liveness status |

Status values: `REAL` (live person), `SPOOF` (fake), `FUZZY` (cannot determine due to image quality), `DETECTING` (still detecting — only in PredictVideo mode).

#### PredictVideo
Determine liveness from continuous video sequence.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| face | const SeetaRect& | | Face position |
| points | const SeetaPointF* | | Landmark array |
| Return | Status | | Liveness status |

#### ResetVideo
Reset liveness detection state for a new PredictVideo session.

#### GetPreFrameScore
Get internal detection scores.

| Parameter | Type | Default | Description |
|---|---|---|---|
| clarity | float* | | Face clarity score |
| reality | float* | | Face liveness score |

#### SetVideoFrameCount / GetVideoFrameCount
Set/get the number of video frames required before returning a liveness result.

#### SetThreshold
Set detection thresholds.

| Parameter | Type | Default | Description |
|---|---|---|---|
| clarity | float | | Face clarity threshold (default: 0.3) |
| reality | float | | Face liveness threshold (default: 0.8) |

#### GetThreshold
Get current thresholds.

#### set / get
Set/get properties. **PROPERTY_NUMBER_THREADS**: thread count, default 4.
