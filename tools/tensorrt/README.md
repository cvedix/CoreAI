# TensorRT Model Conversion and Benchmark Tools

This folder contains scripts and tools for converting YOLOv11 models to TensorRT format and benchmarking their performance.

## Requirements

- Python 3.8+
- CUDA 12.x
- TensorRT 10.x
- ultralytics (`pip install ultralytics`)
- onnx (`pip install onnx onnxslim onnxruntime-gpu`)

## Quick Start

### 1. Convert Model to TensorRT

```bash
# Basic conversion (FP16)
python convert_to_tensorrt.py path/to/model.pt

# Specify output directory and image size
python convert_to_tensorrt.py path/to/model.pt -o ./engines/ --imgsz 640

# Use FP32 precision (not recommended, slower)
python convert_to_tensorrt.py path/to/model.pt --no-half
```

**Options:**
| Option | Default | Description |
|--------|---------|-------------|
| `--output, -o` | Same as input | Output directory |
| `--imgsz` | 640 | Input image size |
| `--half` | True | Use FP16 precision |
| `--no-half` | False | Use FP32 precision |
| `--device` | 0 | CUDA device ID |
| `--batch` | 1 | Batch size |
| `--workspace` | 4 | Workspace size in GB |

### 2. Benchmark Models

```bash
# Benchmark single model with image
python benchmark.py model.engine --source image.jpg

# Benchmark with video and save output
python benchmark.py model.engine --source video.mp4 --save

# Compare PyTorch vs TensorRT
python benchmark.py model.pt --compare model.engine --source image.jpg

# Run 200 iterations
python benchmark.py model.engine --iterations 200
```

**Options:**
| Option | Default | Description |
|--------|---------|-------------|
| `--source, -s` | Random tensor | Input source (image/video) |
| `--compare, -c` | None | Second model to compare |
| `--iterations, -i` | 100 | Benchmark iterations |
| `--warmup, -w` | 10 | Warmup iterations |
| `--save` | False | Save output video/images |
| `--output, -o` | ./results | Output directory |

## Examples

### Convert YOLOv11 License Plate Model

```bash
# From project root
cd /path/to/core_ai_runtime

# Convert
python tools/tensorrt/convert_to_tensorrt.py \
    cvedix_data/models/pt/yolov11/license-plate-finetune-v1x.pt \
    -o cvedix_data/models/tensorrt/

# Benchmark
python tools/tensorrt/benchmark.py \
    cvedix_data/models/tensorrt/license-plate-finetune-v1x.engine \
    --source cvedix_data/video/plate.mp4 \
    --save
```

### Compare Performance

```bash
python tools/tensorrt/benchmark.py \
    cvedix_data/models/pt/yolov11/license-plate-finetune-v1x.pt \
    --compare cvedix_data/models/tensorrt/license-plate-finetune-v1x.engine \
    --source cvedix_data/test_images/body/2.jpg \
    --iterations 50
```

## Performance Results

### YOLOv11x License Plate Detection

| Model | Precision | Avg Time | FPS | Speedup |
|-------|-----------|----------|-----|---------|
| PyTorch | FP32 | 32.91 ms | 30.4 | 1.0x |
| TensorRT | FP16 | 10.13 ms | 98.7 | **3.25x** |

**Tested on:** NVIDIA GeForce RTX 3080 (20GB), CUDA 12.8, TensorRT 10.9

## File Structure

```
tools/tensorrt/
├── README.md                    # This documentation
├── convert_to_tensorrt.py       # Model conversion script
├── benchmark.py                 # Benchmarking script
└── results/                     # Benchmark output directory
    └── video_benchmark/         # Video inference results
```

## Notes

1. **Engine file compatibility**: TensorRT engines are GPU and driver specific. An engine built on one GPU may not work on another.

2. **Re-export on different hardware**: When deploying to a different GPU, re-run the conversion script on that hardware.

3. **Dynamic batch size**: Current scripts use fixed batch size of 1. For dynamic batching, modify the export options.

4. **Memory usage**: Large models may require more GPU memory during conversion. Use `--workspace` to adjust.

## Troubleshooting

### "No module named 'ultralytics'"
```bash
pip install ultralytics --break-system-packages
```

### "No module named 'onnx'"
```bash
pip install onnx onnxslim onnxruntime-gpu --break-system-packages
```

### "Engine deserialization failed"
The engine was built on a different GPU/driver version. Re-export the model.

### Out of memory during conversion
Reduce workspace size or close other GPU applications:
```bash
python convert_to_tensorrt.py model.pt --workspace 2
```
