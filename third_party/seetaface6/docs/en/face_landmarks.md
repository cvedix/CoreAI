# Facial Landmark Detector

## **1. Interface Overview**

The facial landmark detector requires input of original image data and face position, and returns the coordinates of 5 or more facial landmarks (the number depends on the loaded model).

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

## 3. class FaceLandmarker

### 3.1 Constructor

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Detector configuration |

### 3.2 Member Functions

#### number
Get the number of landmarks for the current model. Returns `int`.

#### mark
Get facial landmarks.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image data |
| face | const SeetaRect& | | Face position |
| points | SeetaPointF* | | Output landmark array (must be pre-allocated with length from `number()`) |
| Return | void | | |

#### mark (with occlusion)
Get facial landmarks and occlusion information.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image data |
| face | const SeetaRect& | | Face position |
| points | SeetaPointF* | | Output landmark array |
| mask | int32_t* | | Output occlusion array (1=occluded, 0=not occluded) |
| Return | void | | |

#### mark (vector return)
Get facial landmarks as a vector.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image data |
| face | const SeetaRect& | | Face position |
| Return | std::vector\<SeetaPointF\> | | Landmark array |

#### mark_v2
Get landmarks with occlusion info as PointWithMask vector.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image data |
| face | const SeetaRect& | | Face position |
| Return | std::vector\<PointWithMask\> | | Landmarks with occlusion info |
