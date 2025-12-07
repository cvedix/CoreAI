# InsightFace Model Preparation Guide

This guide explains how to prepare InsightFace models for use with the TensorRT-accelerated face recognition node.

## Overview

The `cvedix_trt_insight_face_recognition_node` requires a TensorRT engine file (.engine) converted from an InsightFace ONNX model. This document provides step-by-step instructions.

## Step 1: Download InsightFace Model

### Option A: Download Pre-trained Models

Download from the [InsightFace Model Zoo](https://github.com/deepinsight/insightface):

```bash
# Large model (ResNet50, ~166MB ONNX, best accuracy)
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip
unzip buffalo_l.zip
# Model: buffalo_l/w600k_r50.onnx

# Medium model (ResNet50, smaller)
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_m.zip
unzip buffalo_m.zip

# Small model (MobileFaceNet, ~4MB ONNX, fastest)
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_s.zip
unzip buffalo_s.zip
```

### Option B: Export from Python InsightFace

If you have a custom trained model:

```python
import insightface
from insightface.app import FaceAnalysis

# Load model
app = FaceAnalysis(name='buffalo_l')
app.prepare(ctx_id=0, det_size=(640, 640))

# Export to ONNX (if supported by your version)
# Or use onnxruntime to convert
```

## Step 2: Verify Model Format

Check the ONNX model structure:

```bash
# Using trtexec (recommended)
/usr/local/tensorRT/bin/trtexec --onnx=w600k_r50.onnx --verbose

# Or using Python
python -c "
import onnx
model = onnx.load('w600k_r50.onnx')
print('Input:', [i.name for i in model.graph.input])
print('Output:', [o.name for o in model.graph.output])
for i in model.graph.input:
    print(f'  {i.name}: {[d.dim_value for d in i.type.tensor_type.shape.dim]}')
"
```

Expected output:
- **Input**: `data` with shape `[batch, 3, 112, 112]` (NCHW)
- **Output**: `fc1` or similar with shape `[batch, 512]`

If your model uses different tensor names, update `third_party/trt_insightface/models/insight_face_recognition.h`:
```cpp
static constexpr const char* kInputTensorName = "your_input_name";
static constexpr const char* kOutputTensorName = "your_output_name";
```

## Step 3: Convert ONNX to TensorRT Engine

### Basic Conversion (FP32)

```bash
/usr/local/tensorRT/bin/trtexec \
  --onnx=w600k_r50.onnx \
  --saveEngine=arcface_r50_fp32.engine \
  --workspace=4096 \
  --minShapes=input:1x3x112x112 \
  --optShapes=input:8x3x112x112 \
  --maxShapes=input:16x3x112x112
```

### FP16 Conversion (Recommended)

Faster inference with minimal accuracy loss:

```bash
/usr/local/tensorRT/bin/trtexec \
  --onnx=w600k_r50.onnx \
  --saveEngine=arcface_r50_fp16.engine \
  --fp16 \
  --workspace=4096 \
  --minShapes=input:1x3x112x112 \
  --optShapes=input:8x3x112x112 \
  --maxShapes=input:16x3x112x112
```

### INT8 Conversion (Fastest, requires calibration)

For maximum speed (may require calibration dataset):

```bash
/usr/local/tensorRT/bin/trtexec \
  --onnx=w600k_r50.onnx \
  --saveEngine=arcface_r50_int8.engine \
  --int8 \
  --workspace=4096 \
  --minShapes=input:1x3x112x112 \
  --optShapes=input:8x3x112x112 \
  --maxShapes=input:16x3x112x112
```

**Note**: Replace `input` with your actual input tensor name if different.

## Step 4: Verify Engine File

Test the engine file:

```bash
# Build test sample
cd build
cmake .. -DCVEDIX_WITH_TRT=ON
make face_recognition_test

# Run test
./samples/face_recognition_test ./arcface_r50_fp16.engine test_face1.jpg test_face2.jpg
```

Expected output:
- Embeddings extracted successfully
- Cosine similarity between same person > 0.6
- Cosine similarity between different persons < 0.4

## Step 5: Deploy Model

Copy the engine file to your model directory:

```bash
mkdir -p ./cvedix_data/models/face
cp arcface_r50_fp16.engine ./cvedix_data/models/face/
```

## Model Comparison

| Model | Size (ONNX) | Accuracy | Speed (RTX 3080) | Use Case |
|-------|-------------|----------|------------------|----------|
| buffalo_l (ResNet100) | ~260MB | Highest | ~400 FPS | Production, high accuracy |
| buffalo_l (ResNet50) | ~166MB | High | ~500 FPS | Balanced |
| buffalo_m | ~50MB | Medium | ~600 FPS | Faster inference |
| buffalo_s (MobileFaceNet) | ~4MB | Good | ~800 FPS | Edge devices, real-time |

## Troubleshooting

### Error: "Cannot read engine file"
- Check file path and permissions
- Verify engine file is not corrupted

### Error: "Wrong input/output tensor names"
- Check tensor names using `trtexec --verbose`
- Update `kInputTensorName` and `kOutputTensorName` in the header

### Low accuracy
- Ensure faces are properly aligned (5-point landmarks)
- Verify preprocessing: (pixel - 127.5) / 128.0
- Check input images are RGB (not BGR)

### CUDA out of memory
- Reduce batch size in `insight_face_recognition.h` (`kBatchSize`)
- Use FP16 or INT8 precision
- Reduce `maxShapes` in trtexec

## Additional Resources

- [InsightFace GitHub](https://github.com/deepinsight/insightface)
- [TensorRT Developer Guide](https://docs.nvidia.com/deeplearning/tensorrt/)
- [ONNX to TensorRT Conversion](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#onnx)






