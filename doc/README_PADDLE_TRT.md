# PaddleOCR with TensorRT 10 Setup Guide

## Overview
You requested to run PaddleOCR optimization on TensorRT 10 hardware.
The code has been updated to support `use_tensorrt` and `precision` configuration.
However, to run this, you must ensure the `paddle_inference` library is compatible with TensorRT 10.

## Compatibility Warning
Official PaddleOCR pre-built binaries often support up to **TensorRT 8.6** (with CUDA 11.8).
**TensorRT 10 is not yet officially supported** in standard pre-built releases (as of early 2025).
To use TensorRT 10, you likely need to **build Paddle Inference from source** against your version of TensorRT 10 and CUDA.

## Setup Instructions

### 1. Build Paddle Inference from Source (Required for TRT 10)
If you cannot find a pre-built release for your environment:
1.  Clone PaddlePaddle source code.
2.  Configure CMake with `-DWITH_TENSORRT=ON` and point to your TensorRT 10 installation.
3.  Build the inference library (`libpaddle_inference.so`).

### 2. Install Library
Once built/downloaded, copy the artifacts to system paths (or update `third_party/paddle_ocr/CMakeLists.txt`):
```bash
# Example paths
sudo cp -r paddle_inference_install_dir/paddle/include/* /usr/local/include/
sudo cp -r paddle_inference_install_dir/paddle/lib/* /usr/local/lib/
```

### 3. Build cvedix Runtime
After installing the library, build the project:
```bash
cd /home/cvedix/Documents/core_ai_runtime
mkdir -p build && cd build
cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_PADDLE=ON ..
make plate_recognition_pipeline_sample
```

### 4. Run Optimization
Run the sample with TensorRT enabled (default in the updated sample):
```bash
./bin/plate_recognition_pipeline_sample \
    ./cvedix_data/test_video/plate.mp4 \
    ./cvedix_data/models/tensorrt/license-plate-finetune-v1n.engine \
    ./cvedix_data/models/paddle/ocr
```

## Performance Tuning
In `nodes/infers/cvedix_plate_recogniton_ppocr3.cpp`, you can adjust the precision:
- `fp16` (Default): Good balance of speed and accuracy.
- `int8`: Fastest, requires calibration or quantization.
- `fp32`: Highest accuracy, slower.
