#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}========================================================${NC}"
echo -e "${CYAN}  TensorRT Detection Bundle Builder (Full Dependencies) ${NC}"
echo -e "${CYAN}  (Plate + Face Detection with CUDA/TRT/OpenCV)         ${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""

# Configuration
BUNDLE_NAME="cvedix-trt-detection-bundle-full"
VERSION="$(date +%Y.%m.%d)"  # Auto-generate version based on current date
BUILD_DIR="${PWD}/build"
BUNDLE_DIR="${PWD}/bundle_output_full"
TRT_MODELS_DIR="${PWD}/cvedix_data/models/trt"

# Dependency paths (adjust for your system)
CUDA_LIB_DIR="/usr/local/cuda/lib64"
TRT_LIB_DIR="/usr/lib/x86_64-linux-gnu"
CUDNN_LIB_DIR="/usr/lib/x86_64-linux-gnu"

# Check prerequisites
echo -e "${BLUE}[1/8] Checking prerequisites...${NC}"

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found. Please build the project first.${NC}"
    exit 1
fi

if [ ! -f "$BUILD_DIR/libs/libtrt_yolov11_face.so" ]; then
    echo -e "${RED}Error: TRT Face library not built.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Create bundle directory structure
echo -e "${BLUE}[2/8] Creating bundle directory structure...${NC}"

rm -rf "$BUNDLE_DIR"
mkdir -p "$BUNDLE_DIR/lib/cuda"
mkdir -p "$BUNDLE_DIR/lib/tensorrt"
mkdir -p "$BUNDLE_DIR/lib/cudnn"
mkdir -p "$BUNDLE_DIR/lib/opencv"
mkdir -p "$BUNDLE_DIR/lib/cvedix"
mkdir -p "$BUNDLE_DIR/bin"
mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/infers"
mkdir -p "$BUNDLE_DIR/include/third_party/trt_yolov11"
mkdir -p "$BUNDLE_DIR/include/third_party/trt_yolov11_face"
mkdir -p "$BUNDLE_DIR/models/trt/plate"
mkdir -p "$BUNDLE_DIR/models/trt/face"
mkdir -p "$BUNDLE_DIR/models/onnx/face"
mkdir -p "$BUNDLE_DIR/video"

echo -e "${GREEN}✓ Directory structure created${NC}"
echo ""

# Copy CVEDIX libraries
echo -e "${BLUE}[3/8] Copying CVEDIX libraries...${NC}"

cp "$BUILD_DIR/libs/libcvedix_core.so" "$BUNDLE_DIR/lib/cvedix/" 2>/dev/null || \
   cp "$BUILD_DIR/libs/libcvedix_instance_sdk.so" "$BUNDLE_DIR/lib/cvedix/" 2>/dev/null || true
cp "$BUILD_DIR/libs/libtrt_yolov11.so" "$BUNDLE_DIR/lib/cvedix/"
cp "$BUILD_DIR/libs/libtrt_yolov11_face.so" "$BUNDLE_DIR/lib/cvedix/"
cp "$BUILD_DIR/libs/libpaddle_ocr.so" "$BUNDLE_DIR/lib/cvedix/" 2>/dev/null || true
cp "$BUILD_DIR/libs/libtinyexpr.so" "$BUNDLE_DIR/lib/cvedix/" 2>/dev/null || true

echo -e "${GREEN}✓ CVEDIX libraries copied${NC}"
echo ""

# Copy CUDA runtime libraries (essential only)
echo -e "${BLUE}[4/8] Copying CUDA libraries...${NC}"

CUDA_LIBS=(
    "libcudart.so*"
    "libcublas.so*"
    "libcublasLt.so*"
    "libnvrtc.so*"
    "libnvrtc-builtins.so*"
)

for lib_pattern in "${CUDA_LIBS[@]}"; do
    for lib in $CUDA_LIB_DIR/$lib_pattern; do
        if [ -f "$lib" ]; then
            cp -a "$lib" "$BUNDLE_DIR/lib/cuda/" 2>/dev/null || true
        fi
    done
done

CUDA_SIZE=$(du -sh "$BUNDLE_DIR/lib/cuda" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ CUDA libs copied (${CUDA_SIZE})${NC}"
echo ""

# Copy TensorRT libraries
echo -e "${BLUE}[5/8] Copying TensorRT libraries...${NC}"

TRT_LIBS=(
    "libnvinfer.so*"
    "libnvinfer_plugin.so*"
    "libnvinfer_lean.so*"
    "libnvinfer_dispatch.so*"
    "libnvinfer_builder_resource.so*"
    "libnvonnxparser.so*"
)

for lib_pattern in "${TRT_LIBS[@]}"; do
    for lib in $TRT_LIB_DIR/$lib_pattern; do
        if [ -f "$lib" ]; then
            cp -a "$lib" "$BUNDLE_DIR/lib/tensorrt/" 2>/dev/null || true
        fi
    done
done

TRT_SIZE=$(du -sh "$BUNDLE_DIR/lib/tensorrt" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ TensorRT libs copied (${TRT_SIZE})${NC}"
echo ""

# Copy cuDNN libraries
echo -e "${BLUE}[6/8] Copying cuDNN libraries...${NC}"

CUDNN_LIBS=(
    "libcudnn.so*"
    "libcudnn_ops.so*"
    "libcudnn_cnn.so*"
    "libcudnn_adv.so*"
    "libcudnn_graph.so*"
    "libcudnn_engines_precompiled.so*"
    "libcudnn_engines_runtime_compiled.so*"
    "libcudnn_heuristic.so*"
)

for lib_pattern in "${CUDNN_LIBS[@]}"; do
    for lib in $CUDNN_LIB_DIR/$lib_pattern; do
        if [ -f "$lib" ]; then
            cp -a "$lib" "$BUNDLE_DIR/lib/cudnn/" 2>/dev/null || true
        fi
    done
done

CUDNN_SIZE=$(du -sh "$BUNDLE_DIR/lib/cudnn" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ cuDNN libs copied (${CUDNN_SIZE})${NC}"
echo ""

# Copy OpenCV libraries
echo -e "${BLUE}[7/8] Copying OpenCV libraries...${NC}"

# Find OpenCV libs
OPENCV_LIB_DIR="/usr/lib/x86_64-linux-gnu"
OPENCV_LIBS=(
    "libopencv_core.so*"
    "libopencv_imgproc.so*"
    "libopencv_imgcodecs.so*"
    "libopencv_highgui.so*"
    "libopencv_videoio.so*"
    "libopencv_dnn.so*"
    "libopencv_calib3d.so*"
    "libopencv_features2d.so*"
    "libopencv_freetype.so*"
    "libopencv_objdetect.so*"
)

for lib_pattern in "${OPENCV_LIBS[@]}"; do
    for lib in $OPENCV_LIB_DIR/$lib_pattern; do
        if [ -f "$lib" ]; then
            cp -a "$lib" "$BUNDLE_DIR/lib/opencv/" 2>/dev/null || true
        fi
    done
done

OPENCV_SIZE=$(du -sh "$BUNDLE_DIR/lib/opencv" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ OpenCV libs copied (${OPENCV_SIZE})${NC}"
echo ""

# Copy binaries, headers, models
echo -e "${BLUE}[8/8] Copying binaries, models, and assets...${NC}"

# Use output/include if available (complete structured headers)
if [ -d "${PWD}/output/include/cvedix" ]; then
    cp -r "${PWD}/output/include/cvedix" "$BUNDLE_DIR/include/"
    echo -e "  ${GREEN}✓ Headers copied from output/include (complete structure)${NC}"
else
    # Fallback: Create include directory structure manually
    mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/src"
    mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/des"
    mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/osd"
    mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/infers/base"
    mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/transform"
    mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/tracker"
    mkdir -p "$BUNDLE_DIR/include/cvedix/objects/shapes"
    mkdir -p "$BUNDLE_DIR/include/cvedix/objects/ba"
    mkdir -p "$BUNDLE_DIR/include/cvedix/utils"

    cp "${PWD}/nodes/src/"*.h "$BUNDLE_DIR/include/cvedix/nodes/src/" 2>/dev/null || true
    cp "${PWD}/nodes/des/"*.h "$BUNDLE_DIR/include/cvedix/nodes/des/" 2>/dev/null || true
    cp "${PWD}/nodes/osd/"*.h "$BUNDLE_DIR/include/cvedix/nodes/osd/" 2>/dev/null || true
    cp "${PWD}/nodes/infers/"*.h "$BUNDLE_DIR/include/cvedix/nodes/infers/" 2>/dev/null || true
    cp "${PWD}/nodes/infers/base/"*.h "$BUNDLE_DIR/include/cvedix/nodes/infers/base/" 2>/dev/null || true
    cp "${PWD}/nodes/transform/"*.h "$BUNDLE_DIR/include/cvedix/nodes/transform/" 2>/dev/null || true
    cp "${PWD}/nodes/tracker/"*.h "$BUNDLE_DIR/include/cvedix/nodes/tracker/" 2>/dev/null || true
    cp "${PWD}/nodes/cvedix_node.h" "$BUNDLE_DIR/include/cvedix/nodes/" 2>/dev/null || true
    cp "${PWD}/objects/"*.h "$BUNDLE_DIR/include/cvedix/objects/" 2>/dev/null || true
    cp "${PWD}/objects/shapes/"*.h "$BUNDLE_DIR/include/cvedix/objects/shapes/" 2>/dev/null || true
    cp "${PWD}/objects/ba/"*.h "$BUNDLE_DIR/include/cvedix/objects/ba/" 2>/dev/null || true
    find "${PWD}/utils" -name "*.h" -exec cp {} "$BUNDLE_DIR/include/cvedix/utils/" \; 2>/dev/null || true
fi

# Third-party headers
mkdir -p "$BUNDLE_DIR/include/third_party/trt_yolov11"
mkdir -p "$BUNDLE_DIR/include/third_party/trt_yolov11_face"
mkdir -p "$BUNDLE_DIR/include/third_party/paddle_ocr"
cp "${PWD}/third_party/trt_yolov11/"*.h "$BUNDLE_DIR/include/third_party/trt_yolov11/" 2>/dev/null || true
cp "${PWD}/third_party/trt_yolov11_face/"*.h "$BUNDLE_DIR/include/third_party/trt_yolov11_face/" 2>/dev/null || true
cp "${PWD}/third_party/paddle_ocr/include/"*.h "$BUNDLE_DIR/include/third_party/paddle_ocr/" 2>/dev/null || true

HEADER_COUNT=$(find "$BUNDLE_DIR/include" -name "*.h" 2>/dev/null | wc -l)
echo -e "  ${GREEN}✓ $HEADER_COUNT header files copied${NC}"

# Binaries
cp "$BUILD_DIR/bin/yolov11_plate_detector_trt_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true
cp "$BUILD_DIR/bin/yolov11_face_detector_trt_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true
cp "$BUILD_DIR/bin/yolov11_face_detector_video_output_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true
cp "$BUILD_DIR/bin/plate_recognition_video_output_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true

# TRT Models
cp "$TRT_MODELS_DIR/face/"*.engine "$BUNDLE_DIR/models/trt/face/" 2>/dev/null || true
cp "${PWD}/cvedix_data/models/tensorrt/"*plate*.engine "$BUNDLE_DIR/models/trt/plate/" 2>/dev/null || true

# ONNX Models
cp "${PWD}/cvedix_data/models/face/face_detection_yolov11.onnx" "$BUNDLE_DIR/models/onnx/face/" 2>/dev/null || true
cp "${PWD}/cvedix_data/models/face/face_detection_yolov11_fp16.onnx" "$BUNDLE_DIR/models/onnx/face/" 2>/dev/null || true

# Test videos
cp "${PWD}/cvedix_data/video/face.mp4" "$BUNDLE_DIR/video/" 2>/dev/null || true
cp "${PWD}/cvedix_data/video/vietnam_plate.mp4" "$BUNDLE_DIR/video/" 2>/dev/null || true

echo -e "${GREEN}✓ Binaries, models, and assets copied${NC}"
echo ""

# Create setup script
cat > "$BUNDLE_DIR/setup_env.sh" << 'SETUP_EOF'
#!/bin/bash
# Setup environment for CVEDIX TRT Detection Bundle (Full Dependencies)

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Add all library paths
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib/cvedix:$SCRIPT_DIR/lib/cuda:$SCRIPT_DIR/lib/tensorrt:$SCRIPT_DIR/lib/cudnn:$SCRIPT_DIR/lib/opencv:$LD_LIBRARY_PATH"
export PATH="$SCRIPT_DIR/bin:$PATH"

echo "========================================"
echo "CVEDIX TRT Detection Bundle - Activated"
echo "========================================"
echo ""
echo "LD_LIBRARY_PATH includes:"
echo "  - $SCRIPT_DIR/lib/cvedix"
echo "  - $SCRIPT_DIR/lib/cuda"
echo "  - $SCRIPT_DIR/lib/tensorrt"
echo "  - $SCRIPT_DIR/lib/cudnn"
echo "  - $SCRIPT_DIR/lib/opencv"
echo ""
echo "Available commands:"
echo "  - yolov11_face_detector_trt_sample"
echo "  - yolov11_face_detector_video_output_sample"  
echo "  - yolov11_plate_detector_trt_sample"
echo ""
echo "Usage:"
echo "  ./bin/yolov11_face_detector_trt_sample ./models/trt/face/yolov11_face_fp16.engine ./video/face.mp4"
echo ""
SETUP_EOF
chmod +x "$BUNDLE_DIR/setup_env.sh"

# Create README
cat > "$BUNDLE_DIR/README.md" << 'README_EOF'
# CVEDIX TensorRT Detection Bundle (Full Dependencies)

## Đây là gì?

Bundle đầy đủ với TẤT CẢ dependencies cần thiết:
- CUDA Runtime
- cuDNN
- TensorRT
- OpenCV
- CVEDIX Detection Libraries

## Yêu cầu tối thiểu

- NVIDIA GPU (compute capability >= 6.0)
- NVIDIA Driver >= 535
- Linux x86_64

**KHÔNG CẦN** cài thêm CUDA, TensorRT, OpenCV!

## Sử dụng nhanh

```bash
# 1. Giải nén
tar -xzf cvedix-trt-detection-bundle-full-*.tar.gz
cd bundle_output_full

# 2. Setup environment
source setup_env.sh

# 3. Chạy face detection
./bin/yolov11_face_detector_trt_sample \
    ./models/trt/face/yolov11_face_fp16.engine \
    ./video/face.mp4

# 4. Chạy với video output
./bin/yolov11_face_detector_video_output_sample \
    ./models/trt/face/yolov11_face_fp16.engine \
    ./video/face.mp4 \
    ./output
```

## Cấu trúc thư mục

```
bundle_output_full/
├── lib/
│   ├── cvedix/       # CVEDIX core + detection libs
│   ├── cuda/         # CUDA runtime libs
│   ├── tensorrt/     # TensorRT libs
│   ├── cudnn/        # cuDNN libs
│   └── opencv/       # OpenCV libs
├── bin/              # Sample executables
├── models/
│   ├── trt/          # Ready-to-use TensorRT engines
│   └── onnx/         # ONNX models for conversion
├── include/          # C++ headers
├── video/       # Sample videos
├── setup_env.sh      # Environment setup script
└── README.md
```

## Performance

| Model | GPU | FPS |
|-------|-----|-----|
| YOLOv11 Face FP16 | RTX 3080 | ~925 |
| YOLOv11 Plate FP16 | RTX 3080 | ~900 |

## Troubleshooting

### Library not found
```bash
source setup_env.sh
```

### CUDA driver mismatch
Cần NVIDIA Driver >= 535

### GPU out of memory
Giảm batch size hoặc sử dụng model nhỏ hơn

README_EOF

# Create VERSION file
cat > "$BUNDLE_DIR/VERSION" << VERSION_EOF
BUNDLE_NAME=$BUNDLE_NAME
VERSION=$VERSION
BUILD_DATE=$(date +"%Y-%m-%d %H:%M:%S")
BUILD_TIMESTAMP=$(date +%s)
ARCHITECTURE=$(uname -m)
PLATFORM=$(uname -s)
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
CUDA_VERSION=$(nvcc --version 2>/dev/null | grep "release" | awk '{print $6}' | tr -d ',' || echo "unknown")
TRT_VERSION=$(dpkg -l | grep tensorrt | head -1 | awk '{print $3}' || echo "unknown")
VERSION_EOF

echo -e "${GREEN}✓ VERSION file created${NC}"

# Create tarball
echo -e "${BLUE}Creating tarball (this may take a while)...${NC}"
TARBALL="${BUNDLE_NAME}-${VERSION}-$(uname -m).tar.gz"
cd "$(dirname $BUNDLE_DIR)"
tar -czf "$TARBALL" "$(basename $BUNDLE_DIR)"
mv "$TARBALL" "${PWD}/" 2>/dev/null || true

# Summary
echo ""
echo -e "${CYAN}========================================================${NC}"
echo -e "${GREEN}Full Bundle created successfully!${NC}"
echo -e "${CYAN}========================================================${NC}"
echo ""

# Calculate sizes
BUNDLE_SIZE=$(du -sh "$BUNDLE_DIR" | cut -f1)
TARBALL_PATH="${PWD}/${TARBALL}"
if [ -f "$TARBALL_PATH" ]; then
    TARBALL_SIZE=$(du -sh "$TARBALL_PATH" | cut -f1)
else
    TARBALL_PATH="${PWD}/$(basename $BUNDLE_DIR)/../${TARBALL}"
    TARBALL_SIZE=$(du -sh "$TARBALL_PATH" 2>/dev/null | cut -f1 || echo "N/A")
fi

echo -e "Bundle directory: ${YELLOW}$BUNDLE_DIR${NC}"
echo -e "Bundle size: ${YELLOW}$BUNDLE_SIZE${NC}"
echo ""

echo -e "${BLUE}Library sizes:${NC}"
echo -e "  CVEDIX:    $(du -sh "$BUNDLE_DIR/lib/cvedix" 2>/dev/null | cut -f1)"
echo -e "  CUDA:      $(du -sh "$BUNDLE_DIR/lib/cuda" 2>/dev/null | cut -f1)"
echo -e "  TensorRT:  $(du -sh "$BUNDLE_DIR/lib/tensorrt" 2>/dev/null | cut -f1)"
echo -e "  cuDNN:     $(du -sh "$BUNDLE_DIR/lib/cudnn" 2>/dev/null | cut -f1)"
echo -e "  OpenCV:    $(du -sh "$BUNDLE_DIR/lib/opencv" 2>/dev/null | cut -f1)"
echo ""

echo -e "${GREEN}Done!${NC}"
