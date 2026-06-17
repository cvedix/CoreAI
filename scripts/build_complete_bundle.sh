#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

echo -e "${MAGENTA}================================================================${NC}"
echo -e "${MAGENTA}  CVEDIX AI Runtime - Complete Project Bundle Builder          ${NC}"
echo -e "${MAGENTA}  (All Libraries, Samples, Models, Dependencies)               ${NC}"
echo -e "${MAGENTA}================================================================${NC}"
echo ""

# Configuration
BUNDLE_NAME="cvedix-ai-runtime-complete"
VERSION="$(date +%Y.%m.%d)"
BUILD_DIR="${PWD}/build"
BUNDLE_DIR="${PWD}/bundle_complete"
CVEDIX_DATA_DIR="${PWD}/cvedix_data"

# Dependency paths
CUDA_LIB_DIR="/usr/local/cuda/lib64"
TRT_LIB_DIR="/usr/lib/x86_64-linux-gnu"
CUDNN_LIB_DIR="/usr/lib/x86_64-linux-gnu"
OPENCV_LIB_DIR="/usr/lib/x86_64-linux-gnu"

echo -e "${BLUE}Configuration:${NC}"
echo -e "  Bundle Name: ${YELLOW}$BUNDLE_NAME${NC}"
echo -e "  Version:     ${YELLOW}$VERSION${NC}"
echo ""

# Check prerequisites
echo -e "${BLUE}[1/10] Checking prerequisites...${NC}"

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found. Please build the project first.${NC}"
    exit 1
fi

if [ ! -f "$BUILD_DIR/libs/libcvedix_core.so" ]; then
    echo -e "${RED}Error: libcvedix_core.so not found. Please build the project.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Create directory structure
echo -e "${BLUE}[2/10] Creating bundle directory structure...${NC}"

rm -rf "$BUNDLE_DIR"
mkdir -p "$BUNDLE_DIR/lib/cvedix"
mkdir -p "$BUNDLE_DIR/lib/cuda"
mkdir -p "$BUNDLE_DIR/lib/tensorrt"
mkdir -p "$BUNDLE_DIR/lib/cudnn"
mkdir -p "$BUNDLE_DIR/lib/opencv"
mkdir -p "$BUNDLE_DIR/lib/paddle"
mkdir -p "$BUNDLE_DIR/bin"
mkdir -p "$BUNDLE_DIR/include"
mkdir -p "$BUNDLE_DIR/models"
mkdir -p "$BUNDLE_DIR/video"
mkdir -p "$BUNDLE_DIR/config"

echo -e "${GREEN}✓ Directory structure created${NC}"
echo ""

# Copy CVEDIX libraries
echo -e "${BLUE}[3/10] Copying CVEDIX libraries...${NC}"

cp "$BUILD_DIR/libs/"*.so "$BUNDLE_DIR/lib/cvedix/" 2>/dev/null || true

CVEDIX_SIZE=$(du -sh "$BUNDLE_DIR/lib/cvedix" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ CVEDIX libs copied (${CVEDIX_SIZE})${NC}"
echo ""

# Copy all sample binaries
echo -e "${BLUE}[4/10] Copying sample binaries...${NC}"

for bin in "$BUILD_DIR/bin/"*; do
    if [ -f "$bin" ] && [ -x "$bin" ]; then
        cp "$bin" "$BUNDLE_DIR/bin/" 2>/dev/null || true
    fi
done

BIN_COUNT=$(ls -1 "$BUNDLE_DIR/bin/" 2>/dev/null | wc -l)
echo -e "  ${GREEN}✓ $BIN_COUNT executables copied${NC}"
echo ""

# Copy ALL header files
echo -e "${BLUE}[5/11] Copying header files...${NC}"

# Use output/include if available (complete structured headers)
if [ -d "${PWD}/output/include/cvedix" ]; then
    cp -r "${PWD}/output/include/cvedix" "$BUNDLE_DIR/include/"
    echo -e "  ${GREEN}✓ Headers copied from output/include (complete structure)${NC}"
else
    # Fallback: manual copy
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
mkdir -p "$BUNDLE_DIR/include/third_party/trt_yolov12"
mkdir -p "$BUNDLE_DIR/include/third_party/trt_rf_detr"
mkdir -p "$BUNDLE_DIR/include/third_party/paddle_ocr"

cp "${PWD}/third_party/trt_yolov11/"*.h "$BUNDLE_DIR/include/third_party/trt_yolov11/" 2>/dev/null || true
cp "${PWD}/third_party/trt_yolov11_face/"*.h "$BUNDLE_DIR/include/third_party/trt_yolov11_face/" 2>/dev/null || true
cp "${PWD}/third_party/trt_yolov12/"*.h "$BUNDLE_DIR/include/third_party/trt_yolov12/" 2>/dev/null || true
cp "${PWD}/third_party/trt_rf_detr/"*.h "$BUNDLE_DIR/include/third_party/trt_rf_detr/" 2>/dev/null || true
cp "${PWD}/third_party/paddle_ocr/include/"*.h "$BUNDLE_DIR/include/third_party/paddle_ocr/" 2>/dev/null || true

HEADER_COUNT=$(find "$BUNDLE_DIR/include" -name "*.h" 2>/dev/null | wc -l)
echo -e "  ${GREEN}✓ $HEADER_COUNT header files copied${NC}"
echo ""

# Copy CUDA libraries
echo -e "${BLUE}[6/11] Copying CUDA libraries...${NC}"

CUDA_LIBS=(
    "libcudart.so*"
    "libcublas.so*"
    "libcublasLt.so*"
    "libnvrtc.so*"
    "libnvrtc-builtins.so*"
    "libcurand.so*"
    "libcusolver.so*"
    "libcusparse.so*"
)

for lib_pattern in "${CUDA_LIBS[@]}"; do
    for lib in $CUDA_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$BUNDLE_DIR/lib/cuda/" 2>/dev/null || true
    done
done

CUDA_SIZE=$(du -sh "$BUNDLE_DIR/lib/cuda" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ CUDA libs copied (${CUDA_SIZE})${NC}"
echo ""

# Copy TensorRT libraries
echo -e "${BLUE}[7/11] Copying TensorRT libraries...${NC}"

TRT_LIBS=(
    "libnvinfer.so*"
    "libnvinfer_plugin.so*"
    "libnvinfer_lean.so*"
    "libnvinfer_dispatch.so*"
    "libnvinfer_builder_resource.so*"
    "libnvonnxparser.so*"
    "libnvparsers.so*"
)

for lib_pattern in "${TRT_LIBS[@]}"; do
    for lib in $TRT_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$BUNDLE_DIR/lib/tensorrt/" 2>/dev/null || true
    done
done

TRT_SIZE=$(du -sh "$BUNDLE_DIR/lib/tensorrt" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ TensorRT libs copied (${TRT_SIZE})${NC}"
echo ""

# Copy cuDNN libraries
echo -e "${BLUE}[8/11] Copying cuDNN libraries...${NC}"

CUDNN_LIBS=(
    "libcudnn.so*"
    "libcudnn_ops.so*"
    "libcudnn_cnn.so*"
    "libcudnn_adv.so*"
    "libcudnn_graph.so*"
    "libcudnn_engines*.so*"
    "libcudnn_heuristic.so*"
)

for lib_pattern in "${CUDNN_LIBS[@]}"; do
    for lib in $CUDNN_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$BUNDLE_DIR/lib/cudnn/" 2>/dev/null || true
    done
done

CUDNN_SIZE=$(du -sh "$BUNDLE_DIR/lib/cudnn" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ cuDNN libs copied (${CUDNN_SIZE})${NC}"
echo ""

# Copy OpenCV libraries
echo -e "${BLUE}[9/11] Copying OpenCV libraries...${NC}"

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
    "libopencv_flann.so*"
)

for lib_pattern in "${OPENCV_LIBS[@]}"; do
    for lib in $OPENCV_LIB_DIR/$lib_pattern; do
        [ -f "$lib" ] && cp -a "$lib" "$BUNDLE_DIR/lib/opencv/" 2>/dev/null || true
    done
done

OPENCV_SIZE=$(du -sh "$BUNDLE_DIR/lib/opencv" 2>/dev/null | cut -f1)
echo -e "  ${GREEN}✓ OpenCV libs copied (${OPENCV_SIZE})${NC}"
echo ""

# Copy Paddle Inference libraries (if exist)
echo -e "${BLUE}[10/11] Copying Paddle Inference libraries...${NC}"

if [ -d "${PWD}/third_party/paddle_inference/lib" ]; then
    cp -a "${PWD}/third_party/paddle_inference/lib/"*.so* "$BUNDLE_DIR/lib/paddle/" 2>/dev/null || true
    cp -a "${PWD}/third_party/paddle_inference/paddle2onnx/lib/"*.so* "$BUNDLE_DIR/lib/paddle/" 2>/dev/null || true
    cp -a "${PWD}/third_party/paddle_inference/mklml/lib/"*.so* "$BUNDLE_DIR/lib/paddle/" 2>/dev/null || true
    PADDLE_SIZE=$(du -sh "$BUNDLE_DIR/lib/paddle" 2>/dev/null | cut -f1)
    echo -e "  ${GREEN}✓ Paddle libs copied (${PADDLE_SIZE})${NC}"
else
    echo -e "  ${YELLOW}⚠ Paddle libs not found, skipping${NC}"
fi
echo ""

# Copy models and data
echo -e "${BLUE}[11/11] Copying models and data...${NC}"

if [ -d "$CVEDIX_DATA_DIR" ]; then
    # Copy all models
    if [ -d "$CVEDIX_DATA_DIR/models" ]; then
        cp -r "$CVEDIX_DATA_DIR/models" "$BUNDLE_DIR/"
        MODELS_SIZE=$(du -sh "$BUNDLE_DIR/models" 2>/dev/null | cut -f1)
        echo -e "  ${GREEN}✓ Models copied (${MODELS_SIZE})${NC}"
    fi
    
    # Copy test videos
    if [ -d "$CVEDIX_DATA_DIR/video" ]; then
        cp -r "$CVEDIX_DATA_DIR/video/"* "$BUNDLE_DIR/video/" 2>/dev/null || true
        VIDEO_SIZE=$(du -sh "$BUNDLE_DIR/video" 2>/dev/null | cut -f1)
        echo -e "  ${GREEN}✓ Test videos copied (${VIDEO_SIZE})${NC}"
    fi
else
    echo -e "  ${YELLOW}⚠ cvedix_data directory not found${NC}"
fi
echo ""

# Create setup script
cat > "$BUNDLE_DIR/setup_env.sh" << 'SETUP_EOF'
#!/bin/bash
# CVEDIX AI Runtime Complete Bundle - Environment Setup

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Add all library paths
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib/cvedix:$SCRIPT_DIR/lib/cuda:$SCRIPT_DIR/lib/tensorrt:$SCRIPT_DIR/lib/cudnn:$SCRIPT_DIR/lib/opencv:$SCRIPT_DIR/lib/paddle:$LD_LIBRARY_PATH"
export PATH="$SCRIPT_DIR/bin:$PATH"

# Print version
if [ -f "$SCRIPT_DIR/VERSION" ]; then
    source "$SCRIPT_DIR/VERSION"
    echo "========================================"
    echo "CVEDIX AI Runtime Complete Bundle"
    echo "Version: $VERSION"
    echo "Build Date: $BUILD_DATE"
    echo "========================================"
fi

echo ""
echo "Environment configured successfully!"
echo ""
echo "LD_LIBRARY_PATH includes:"
ls -d "$SCRIPT_DIR/lib/"*/ 2>/dev/null | while read dir; do
    echo "  - $(basename $dir)"
done
echo ""
echo "Available executables:"
ls "$SCRIPT_DIR/bin/" 2>/dev/null | head -20
echo ""
SETUP_EOF
chmod +x "$BUNDLE_DIR/setup_env.sh"

# Create README
cat > "$BUNDLE_DIR/README.md" << 'README_EOF'
# CVEDIX AI Runtime Complete Bundle

## Đây là gì?

Bundle đầy đủ của CVEDIX AI Runtime SDK bao gồm:
- Tất cả libraries và dependencies
- Tất cả sample executables
- Tất cả models (ONNX, TensorRT, PaddleOCR)
- Test videos

## Yêu cầu tối thiểu

- NVIDIA GPU (compute capability >= 6.0)
- NVIDIA Driver >= 535
- Linux x86_64
- GStreamer 1.0

**KHÔNG CẦN** cài CUDA, TensorRT, OpenCV!

## Sử dụng nhanh

```bash
# 1. Giải nén
tar -xzf cvedix-ai-runtime-complete-*.tar.gz
cd bundle_complete

# 2. Setup environment (BẮT BUỘC)
source setup_env.sh

# 3. Chạy sample
./bin/yolov11_face_detector_trt_sample \
    ./models/trt/face/yolov11_face_fp16.engine \
    ./video/face.mp4
```

## Cấu trúc thư mục

```
bundle_complete/
├── lib/
│   ├── cvedix/       # CVEDIX core + all detection libs
│   ├── cuda/         # CUDA runtime
│   ├── tensorrt/     # TensorRT
│   ├── cudnn/        # cuDNN
│   ├── opencv/       # OpenCV
│   └── paddle/       # Paddle Inference
├── bin/              # All sample executables
├── models/           # All models (ONNX, TRT, etc.)
├── video/       # Test videos
├── include/          # C++ headers
├── config/           # Configurations
├── setup_env.sh      # Environment setup
├── VERSION           # Version info
└── README.md
```

## Components

- **Face Detection**: YOLOv11 TensorRT
- **Plate Detection**: YOLOv11 TensorRT
- **Plate Recognition**: PaddleOCR
- **Face Recognition**: InsightFace TensorRT
- **Object Detection**: YOLOv8/v11

README_EOF

# Create VERSION file
cat > "$BUNDLE_DIR/VERSION" << VERSION_EOF
BUNDLE_NAME=$BUNDLE_NAME
VERSION=$VERSION
BUILD_DATE=$(date +"%Y-%m-%d %H:%M:%S")
BUILD_TIMESTAMP=$(date +%s)
ARCHITECTURE=$(uname -m)
PLATFORM=$(uname -s)
KERNEL=$(uname -r)
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
CUDA_VERSION=$(nvcc --version 2>/dev/null | grep "release" | awk '{print $6}' | tr -d ',' || echo "unknown")
TRT_VERSION=$(cat /usr/include/x86_64-linux-gnu/NvInferVersion.h 2>/dev/null | grep NV_TENSORRT_MAJOR | head -1 | awk '{print $3}' || echo "unknown")
OPENCV_VERSION=$(pkg-config --modversion opencv4 2>/dev/null || echo "unknown")
VERSION_EOF

echo -e "${GREEN}✓ VERSION file created${NC}"
echo ""

# Calculate total size before compression
TOTAL_SIZE=$(du -sh "$BUNDLE_DIR" | cut -f1)
echo -e "${CYAN}Total bundle size (uncompressed): ${YELLOW}$TOTAL_SIZE${NC}"
echo ""

# Create tarball
echo -e "${BLUE}Creating tarball (this may take a while)...${NC}"
TARBALL="${BUNDLE_NAME}-${VERSION}-$(uname -m).tar.gz"
cd "$(dirname $BUNDLE_DIR)"
tar -czf "$TARBALL" "$(basename $BUNDLE_DIR)"

TARBALL_SIZE=$(du -sh "$TARBALL" | cut -f1)

# Summary
echo ""
echo -e "${MAGENTA}================================================================${NC}"
echo -e "${GREEN}Complete Project Bundle created successfully!${NC}"
echo -e "${MAGENTA}================================================================${NC}"
echo ""
echo -e "Bundle directory: ${YELLOW}$BUNDLE_DIR${NC}"
echo -e "Tarball: ${YELLOW}$(pwd)/$TARBALL${NC}"
echo -e "Tarball size: ${YELLOW}$TARBALL_SIZE${NC}"
echo ""

echo -e "${BLUE}Library sizes:${NC}"
for dir in "$BUNDLE_DIR/lib/"*/; do
    dirname=$(basename "$dir")
    size=$(du -sh "$dir" 2>/dev/null | cut -f1)
    printf "  %-12s %s\n" "$dirname:" "$size"
done
echo ""

echo -e "${BLUE}Executables: ${YELLOW}$(ls -1 "$BUNDLE_DIR/bin/" 2>/dev/null | wc -l)${NC}"
echo -e "${BLUE}Models size: ${YELLOW}$(du -sh "$BUNDLE_DIR/models" 2>/dev/null | cut -f1)${NC}"
echo -e "${BLUE}Test videos: ${YELLOW}$(du -sh "$BUNDLE_DIR/video" 2>/dev/null | cut -f1)${NC}"
echo ""

echo -e "${GREEN}Done!${NC}"
