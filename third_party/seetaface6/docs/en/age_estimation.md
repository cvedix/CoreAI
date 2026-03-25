# Age Estimator

## **1. Interface Overview**

The age estimator requires original image data and facial landmarks (or pre-cropped face data), and estimates the age of the input face.

## **2. Type Descriptions**

### **2.1 struct SeetaImageData**

| Name | Type | Description |
|---|---|---|
| data | uint8_t* | Image data |
| width | int32_t | Image width |
| height | int32_t | Image height |
| channels | int32_t | Number of channels |

### **2.2 struct SeetaPointF**

| Name | Type | Description |
|---|---|---|
| x | double | X-coordinate of landmark |
| y | double | Y-coordinate of landmark |

## 3. class AgePredictor

### 3.1 Constructor

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Configuration |

### 3.2 Member Functions

#### GetCropFaceWidth / GetCropFaceHeight / GetCropFaceChannels
Get crop face dimensions. Returns `int`.

#### CropFace
Crop face from image.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| points | const SeetaPointF* | | Landmark array |
| face | SeetaImageData& | | Output cropped face |
| Return | bool | | true if successful |

#### PredictAge
Predict age from a pre-cropped face.

| Parameter | Type | Default | Description |
|---|---|---|---|
| face | const SeetaImageData& | | Cropped face |
| age | int& | | Estimated age |
| Return | bool | | true if successful |

#### PredictAgeWithCrop
Predict age from original image with landmarks.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| points | const SeetaPointF* | | Landmarks |
| age | int& | | Estimated age |
| Return | bool | | true if successful |

#### set / get
Set/get properties. **PROPERTY_NUMBER_THREADS**: thread count, default 4.
