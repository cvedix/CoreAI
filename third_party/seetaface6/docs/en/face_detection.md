# Face Detector

## **1. Interface Overview**

The face detector performs face detection on input color or grayscale images and returns the positions of all detected faces.

## **2. Type Descriptions**

### **2.1 struct SeetaImageData**

| Name | Type | Description |
|---|---|---|
| data | uint8_t* | Image data |
| width | int32_t | Image width |
| height | int32_t | Image height |
| channels | int32_t | Number of image channels |

Note: Stores color (3-channel) or grayscale (1-channel) images. Pixels are stored contiguously in row-major order. Color images use BGR888 format, grayscale images use single-byte values.

### **2.2 struct SeetaRect**

| Name | Type | Description |
|---|---|---|
| x | int32_t | X-coordinate of top-left corner of face region |
| y | int32_t | Y-coordinate of top-left corner of face region |
| width | int32_t | Width of face region |
| height | int32_t | Height of face region |

### **2.3 struct SeetaFaceInfo**

| Name | Type | Description |
|---|---|---|
| pos | SeetaRect | Face position |
| score | float | Face confidence score |

### **2.4 struct SeetaFaceInfoArray**

| Name | Type | Description |
|---|---|---|
| data | const SeetaFaceInfo* | Array of face information |
| size | int | Length of face information array |

## 3. class FaceDetector

Face detector.

### 3.1 Enum SeetaDevice

Computation device for running the model.

| Name | Description |
|---|---|
| SEETA_DEVICE_AUTO | Auto-detect, GPU preferred |
| SEETA_DEVICE_CPU | Use CPU |
| SEETA_DEVICE_GPU | Use GPU |

### 3.2 struct SeetaModelSetting

Parameters required for constructing the face detector.

| Parameter | Type | Default | Description |
|---|---|---|---|
| model | const char** | | Detector model |
| id | int | | GPU id |
| device | SeetaDevice | AUTO | Computation device (CPU or GPU) |

### 3.3 Constructor

#### FaceDetector

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Detector configuration |

### 3.4 Member Functions

#### detect

Detect faces in a color image.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Input image data |
| Return | SeetaFaceInfoArray | | Array of detected face info |

#### set

Set face detector properties:
- **PROPERTY_MIN_FACE_SIZE**: Minimum detectable face size. Smaller values detect smaller faces but slower. Default: 20.
- **PROPERTY_THRESHOLD**: Detection filter threshold. Default: 0.90.
- **PROPERTY_MAX_IMAGE_WIDTH** / **PROPERTY_MAX_IMAGE_HEIGHT**: Maximum supported input image dimensions.
- **PROPERTY_NUMBER_THREADS**: Number of computation threads. Default: 4.

| Parameter | Type | Default | Description |
|---|---|---|---|
| property | Property | | Property type |
| value | double | | Property value |
| Return | void | | |

#### get

Get face detector property values.

| Parameter | Type | Default | Description |
|---|---|---|---|
| property | Property | | Property type |
| Return | double | | Corresponding property value |
