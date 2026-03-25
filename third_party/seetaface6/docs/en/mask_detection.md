# Mask Detector

## **1. Interface Overview**

The mask detector takes image data and face position as input, and returns the detection result of whether the person is wearing a mask.

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

## 3. class MaskDetector

### 3.1 Constructor

| Parameter | Type | Default | Description |
|---|---|---|---|
| setting | const SeetaModelSetting& | | Detector configuration |

### 3.2 Member Functions

#### detect
Detect whether a face is wearing a mask.

| Parameter | Type | Default | Description |
|---|---|---|---|
| image | const SeetaImageData& | | Original image |
| face | const SeetaRect& | | Face position |
| score | float* | nullptr | Mask confidence score |
| Return | bool | | true if wearing a mask |
