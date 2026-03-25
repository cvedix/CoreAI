# Quality Assessor

## **1. Interface Overview**

Quality assessors evaluate the quality of face images from multiple aspects, returning quality levels and scores that help determine if a face image is suitable for recognition.

## **2. Type Descriptions**

### **2.1 struct QualityResult**

| Name | Type | Description |
|---|---|---|
| level | QualityLevel | Quality level: LOW, MEDIUM, HIGH |
| score | float | Quality score |

## 3. class QualityOfBrightness

Non-deep-learning face brightness assessor.

### 3.1 Constructor

Default constructor, or with parameters `(float v0, float v1, float v2, float v3)`.

Grading: `[0, v0)` and `[v3, ~)` → LOW; `[v0, v1)` and `[v2, v3)` → MEDIUM; `[v1, v2)` → HIGH.

### 3.2 Member Functions

#### check

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| face | const SeetaRect& | | Face position |
| points | const SeetaPointF* | | 5-point landmark array |
| N | const int32_t | | Landmark count |
| Return | QualityResult | | Brightness result |

## 4. class QualityOfClarity

Non-deep-learning face clarity assessor.

### 4.1 Constructor

Default constructor, or with parameters `(float low, float high)`.

Grading: `[0, low)` → LOW; `[low, high)` → MEDIUM; `[high, ~)` → HIGH.

### 4.2 Member Functions

#### check
Same signature as QualityOfBrightness. Returns clarity assessment.

## 5. class QualityOfLBN

Deep-learning face clarity assessor (model-based).

### 5.1 Constructor

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Model configuration |

### 5.2 Member Functions

#### Detect

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| points | const SeetaPointF* | | 68-point landmark array |
| light | int* | | Brightness result (not recommended) |
| blur | int* | | Blur result (0=clear, 1=blurry) |
| noise | int* | | Noise result (not recommended) |
| Return | void | | |

#### set / get
Properties:
- **PROPERTY_NUMBER_THREADS**: Thread count, default 4.
- **PROPERTY_ARM_CPU_MODE**: Mobile CPU mode. 0=big core, 1=small core, 2=balanced (default).
- **PROPERTY_BLUR_THRESH**: Blur threshold, default 0.80.

## 6. class QualityOfPose

Non-deep-learning face pose assessor.

### 6.1 Constructor
Default constructor (no parameters).

### 6.2 check
Same signature as QualityOfBrightness. Returns pose quality.

## 7. class QualityOfPoseEx

Deep-learning face pose assessor.

### 7.1 Constructor

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Model configuration |

### 7.2 Member Functions

#### check (basic)
Same signature as QualityOfBrightness. Returns pose quality.

#### check (with angles)
Returns specific pose angles.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| face | const SeetaRect& | | Face position |
| points | const SeetaPointF* | | 5-point landmarks |
| N | const int32_t | | Landmark count |
| yaw | float& | | Yaw angle |
| pitch | float& | | Pitch angle |
| roll | float& | | Roll angle |
| Return | bool | | true if successful |

#### set / get
Properties: `YAW_HIGH_THRESHOLD`, `YAW_LOW_THRESHOLD`, `PITCH_HIGH_THRESHOLD`, `PITCH_LOW_THRESHOLD`, `ROLL_HIGH_THRESHOLD`, `ROLL_LOW_THRESHOLD`.

## 8. class QualityOfResolution

Non-deep-learning face size assessor.

### 8.1 Constructor
Default, or with `(float low, float high)`.

### 8.2 check
Same signature as QualityOfBrightness. Returns resolution quality.

## 9. class QualityOfIntegrity

Non-deep-learning face integrity assessor — evaluates how close the face is to the image edge.

### 9.1 Constructor
Default, or with `(float low, float high)` to control edge proximity tolerance.

### 9.2 check
Same signature as QualityOfBrightness. Returns integrity quality.
