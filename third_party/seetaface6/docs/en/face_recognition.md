# Face Recognizer

## **1. Interface Overview**

The face recognizer requires input of original image data and facial landmark points (or pre-cropped face data), extracts feature vectors from the input face, and compares face similarity based on the extracted feature arrays.

## **2. Type Descriptions**

### **2.1 struct SeetaImageData**

| Name | Type | Description |
|---|---|---|
| data | uint8_t* | Image data |
| width | int32_t | Image width |
| height | int32_t | Image height |
| channels | int32_t | Number of image channels |

Note: Stores color (3-channel) or grayscale (1-channel) images. Pixels stored contiguously in row-major order. Color images use BGR888 format.

### **2.2 struct SeetaPointF**

| Name | Type | Description |
|---|---|---|
| x | double | X-coordinate of facial landmark |
| y | double | Y-coordinate of facial landmark |

## 3. class FaceRecognizer

Face recognizer.

### 3.1 Enum SeetaDevice

| Name | Description |
|---|---|
| SEETA_DEVICE_AUTO | Auto-detect, GPU preferred |
| SEETA_DEVICE_CPU | Use CPU |
| SEETA_DEVICE_GPU | Use GPU |

### 3.2 struct SeetaModelSetting

| Parameter | Type | Default | Description |
|---|---|---|---|
| model | const char** | | Recognizer model |
| id | int | | GPU id |
| device | SeetaDevice | AUTO | Computation device |

### 3.3 Constructor

#### FaceRecognizer

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Recognizer configuration |

### 3.4 Member Functions

#### GetCropFaceWidth
Get the width of the cropped face. Returns `int`.

#### GetCropFaceHeight
Get the height of the cropped face. Returns `int`.

#### GetCropFaceChannels
Get the number of channels of the cropped face data. Returns `int`.

#### CropFace
Crop a face from the image.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image data |
| points | const SeetaPointF* | | Facial landmark array |
| face | SeetaImageData& | | Output cropped face |
| Return | bool | | true if cropping succeeded |

#### CropFaceV2 / GetCropFaceWidthV2 / GetCropFaceHeightV2 / GetCropFaceChannelsV2
V2 variants of the crop functions with instance-level (non-static) behavior.

#### GetExtractFeatureSize
Get the length of the feature vector array. Returns `int`.

#### ExtractCroppedFace
Extract feature vector from a pre-cropped face image.

| Parameter | Type | Default | Description |
|---|---|---|---|
| face | const SeetaImageData& | | Cropped face image |
| features | float* | | Output feature array |
| Return | bool | | true if extraction succeeded |

#### Extract
Extract feature vector from original image using landmark points.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original face image |
| points | const SeetaPointF* | | Facial landmark array |
| features | float* | | Output feature array |
| Return | bool | | true if extraction succeeded |

#### CalculateSimilarity
Compare two face feature arrays and get similarity score.

| Parameter | Type | Default | Description |
|---|---|---|---|
| features1 | const float* | | Feature array 1 |
| features2 | const float* | | Feature array 2 |
| Return | float | | Similarity score |

#### set / get
Set/get properties. **PROPERTY_NUMBER_THREADS**: computation thread count, default 4.
