# TensorRT InsightFace Recognition Library

This library provides TensorRT-accelerated inference for InsightFace face recognition models (typically ArcFace).

## Overview

The library wraps TensorRT inference for InsightFace models, providing:
- High-performance face embedding extraction
- Batch processing support
- L2-normalized embeddings (512-dim by default)
- Easy integration with the CVEDIX pipeline

## Requirements

- TensorRT 8.x or later
- CUDA 11.x or later
- OpenCV 4.6+
- NVIDIA GPU with compute capability >= 6.1

## Model Preparation

### 1. Download InsightFace Model

Download an InsightFace model from the [official repository](https://github.com/deepinsight/insightface):

```bash
# Example: Download buffalo_l (large model with ResNet50)
wget https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_l.zip
unzip buffalo_l.zip
# Model file: buffalo_l/w600k_r50.onnx
```

### 2. Convert ONNX to TensorRT Engine

Use `trtexec` to convert the ONNX model to a TensorRT engine:

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

**Note**: 
- Replace `input` with the actual input tensor name if different
- Adjust batch sizes (`minShapes`, `optShapes`, `maxShapes`) based on your needs
- Use `--fp16` for faster inference with minimal accuracy loss
- Use `--int8` for even faster inference (requires calibration)

### 3. Verify Model Input/Output

The model should have:
- **Input**: `data` tensor with shape `[batch, 3, 112, 112]` (NCHW format)
- **Output**: `fc1` tensor with shape `[batch, 512]` (embedding vector)

If your model uses different tensor names, update them in `models/insight_face_recognition.h`:
```cpp
static constexpr const char* kInputTensorName = "your_input_name";
static constexpr const char* kOutputTensorName = "your_output_name";
```

## Usage

### Standalone Test

Build and run the test sample:

```bash
cd build
cmake .. -DCVEDIX_WITH_TRT=ON
make face_recognition_test
./samples/face_recognition_test ./arcface_r50_fp16.engine face1.jpg face2.jpg
```

### Integration with CVEDIX Pipeline

See the main `nodes/README.md` for pipeline integration examples.

## API Reference

### `InsightFaceRecognition` Class

#### Constructor
```cpp
InsightFaceRecognition(const std::string& engine_path);
```
- `engine_path`: Path to the TensorRT engine file (.engine)

#### Methods

**`extract_features()`**
```cpp
void extract_features(
    const std::vector<cv::Mat>& faces, 
    std::vector<std::vector<float>>& embeddings
);
```
- `faces`: Vector of aligned face images (112x112 RGB)
- `embeddings`: Output vector of embedding vectors (512-dim each, L2-normalized)

**`get_embedding_size()`**
```cpp
int get_embedding_size() const;
```
Returns the embedding dimension (typically 512).

## Performance

Typical performance on NVIDIA GPUs:
- **RTX 3080**: ~500-800 FPS (batch size 8, FP16)
- **RTX 2080**: ~300-500 FPS (batch size 8, FP16)
- **Jetson Xavier NX**: ~50-100 FPS (batch size 4, FP16)

Performance depends on:
- GPU model and compute capability
- Batch size
- Precision (FP32/FP16/INT8)
- Input image resolution

## Notes

- Input images should be **aligned** and **112x112** RGB
- Embeddings are **L2-normalized** for cosine similarity calculations
- The library uses **batch processing** for efficiency
- Thread-safe for multiple instances, but each instance should use a separate GPU context

## Troubleshooting

### Engine file not found
- Verify the engine path is correct
- Check file permissions

### CUDA out of memory
- Reduce batch size in `insight_face_recognition.h` (`kBatchSize`)
- Use FP16 or INT8 precision

### Wrong input/output tensor names
- Check your model's tensor names using `trtexec --onnx=model.onnx --verbose`
- Update `kInputTensorName` and `kOutputTensorName` in the header file

### Low accuracy
- Ensure faces are properly aligned (5-point landmarks)
- Verify preprocessing matches training (normalization: (pixel - 127.5) / 128.0)
- Check that input images are RGB (not BGR)


