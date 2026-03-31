# Eye State Detector

## **1. Interface Overview**

The eye state detector requires original image data and facial landmarks, and returns the state of the left and right eyes.

## **2. Type Descriptions**

### **2.1 struct SeetaImageData** / **2.2 struct SeetaPointF**

Same as other modules.

## 3. class EyeStateDetector

### 3.1 Constructor

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Detector configuration |

### 3.2 Member Functions

#### Detect
Detect eye states from original image and landmarks.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| points | const SeetaPointF* | | Landmark array |
| leftState | EYE_STATE | | Left eye state |
| rightState | EYE_STATE | | Right eye state |

EYE_STATE values:
- `EYE_CLOSE` — Eye closed
- `EYE_OPEN` — Eye open
- `EYE_RANDOM` — Non-eye region
- `EYE_UNKNOWN` — Unknown state

#### set / get
Set/get properties. **PROPERTY_NUMBER_THREADS**: thread count, default 4.
