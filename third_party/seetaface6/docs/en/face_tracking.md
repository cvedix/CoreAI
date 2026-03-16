# Face Tracker

## **1. Interface Overview**

The face tracker tracks faces in input color or grayscale video frames and returns information for all tracked faces.

## **2. Type Descriptions**

### **2.1 struct SeetaImageData**

| Name | Type | Description |
|---|---|---|
| data | uint8_t* | Image data |
| width | int32_t | Image width |
| height | int32_t | Image height |
| channels | int32_t | Number of channels |

Note: Stores color (3-channel) or grayscale (1-channel) images. Row-major, BGR888 format for color.

### **2.2 struct SeetaRect**

| Name | Type | Description |
|---|---|---|
| x | int32_t | X of top-left corner |
| y | int32_t | Y of top-left corner |
| width | int32_t | Face region width |
| height | int32_t | Face region height |

### **2.3 struct SeetaTrackingFaceInfo**

| Name | Type | Description |
|---|---|---|
| pos | SeetaRect | Face position |
| score | float | Confidence score |
| frame_no | int | Video frame index |
| PID | int | Tracking face identity ID |

### **2.4 struct SeetaTrackingFaceInfoArray**

| Name | Type | Description |
|---|---|---|
| data | const SeetaTrackingFaceInfo* | Face info array |
| size | int | Array length |

## 3. class FaceTracker

### 3.1 Constructor

#### FaceTracker

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Tracker configuration |
| video_width | int | | Video frame width |
| video_height | int | | Video frame height |

### 3.2 Member Functions

#### SetSingleCalculationThreads
Set the number of computation threads.

#### Track
Track faces in a video frame.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image data |
| Return | SeetaTrackingFaceInfoArray | | Array of tracked faces |

#### Track (with frame number)

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image data |
| frame_no | int | | Video frame index |
| Return | SeetaTrackingFaceInfoArray | | Array of tracked faces |

#### SetMinFaceSize
Set minimum detectable face size. Must be >= 20. Smaller values detect smaller faces but are slower.

#### GetMinFaceSize
Get current minimum face size. Returns `int32_t`.

#### SetThreshold / GetThreshold
Set/get the detection threshold.

#### SetVideoStable
Enable stable mode for outputting face tracking results. Only use during continuous video tracking.

#### GetVideoStable
Get whether stable mode is enabled. Returns `bool`.
