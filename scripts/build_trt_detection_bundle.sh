#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}===================================================${NC}"
echo -e "${CYAN}  TensorRT Detection Bundle Builder               ${NC}"
echo -e "${CYAN}  (Plate Detection + Face Detection with Models)  ${NC}"
echo -e "${CYAN}===================================================${NC}"
echo ""

# Configuration
BUNDLE_NAME="cvedix-trt-detection-bundle"
VERSION="$(date +%Y.%m.%d)"  # Auto-generate version based on current date
BUILD_DIR="${PWD}/build"
BUNDLE_DIR="${PWD}/bundle_output"
TRT_MODELS_DIR="${PWD}/cvedix_data/models/trt"

# Check prerequisites
echo -e "${BLUE}[1/6] Checking prerequisites...${NC}"

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory not found. Please build the project first:${NC}"
    echo "  mkdir build && cd build"
    echo "  cmake -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON -DCVEDIX_BUILD_SAMPLES=ON .."
    echo "  make -j\$(nproc)"
    exit 1
fi

if [ ! -f "$BUILD_DIR/libs/libtrt_yolov11.so" ]; then
    echo -e "${RED}Error: TRT YOLOv11 library not built. Rebuild with -DCVEDIX_WITH_TRT=ON${NC}"
    exit 1
fi

if [ ! -f "$BUILD_DIR/libs/libtrt_yolov11_face.so" ]; then
    echo -e "${RED}Error: TRT YOLOv11 Face library not built. Rebuild the project.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Prerequisites check passed${NC}"
echo ""

# Create bundle directory structure
echo -e "${BLUE}[2/6] Creating bundle directory structure...${NC}"

rm -rf "$BUNDLE_DIR"
mkdir -p "$BUNDLE_DIR/lib"
mkdir -p "$BUNDLE_DIR/bin"
mkdir -p "$BUNDLE_DIR/include/cvedix/nodes/infers"
mkdir -p "$BUNDLE_DIR/include/third_party/trt_yolov11"
mkdir -p "$BUNDLE_DIR/include/third_party/trt_yolov11_face"
mkdir -p "$BUNDLE_DIR/models/trt/plate"
mkdir -p "$BUNDLE_DIR/models/trt/face"
mkdir -p "$BUNDLE_DIR/models/onnx/plate"
mkdir -p "$BUNDLE_DIR/models/onnx/face"
mkdir -p "$BUNDLE_DIR/samples"
mkdir -p "$BUNDLE_DIR/video"

echo -e "${GREEN}✓ Directory structure created${NC}"
echo ""

# Copy libraries
echo -e "${BLUE}[3/6] Copying libraries...${NC}"

# Core libraries
cp "$BUILD_DIR/libs/libcvedix_core.so" "$BUNDLE_DIR/lib/" 2>/dev/null || \
   cp "$BUILD_DIR/libs/libcvedix_instance_sdk.so" "$BUNDLE_DIR/lib/" 2>/dev/null || true

# TRT detection libraries
cp "$BUILD_DIR/libs/libtrt_yolov11.so" "$BUNDLE_DIR/lib/"
cp "$BUILD_DIR/libs/libtrt_yolov11_face.so" "$BUNDLE_DIR/lib/"

# Paddle OCR if available
cp "$BUILD_DIR/libs/libpaddle_ocr.so" "$BUNDLE_DIR/lib/" 2>/dev/null || true

echo -e "${GREEN}✓ Libraries copied${NC}"
echo ""

# Copy headers
echo -e "${BLUE}[4/6] Copying headers...${NC}"

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
mkdir -p "$BUNDLE_DIR/include/third_party/paddle_ocr"
cp "${PWD}/third_party/trt_yolov11/"*.h "$BUNDLE_DIR/include/third_party/trt_yolov11/" 2>/dev/null || true
cp "${PWD}/third_party/trt_yolov11_face/"*.h "$BUNDLE_DIR/include/third_party/trt_yolov11_face/" 2>/dev/null || true
cp "${PWD}/third_party/paddle_ocr/include/"*.h "$BUNDLE_DIR/include/third_party/paddle_ocr/" 2>/dev/null || true

HEADER_COUNT=$(find "$BUNDLE_DIR/include" -name "*.h" 2>/dev/null | wc -l)
echo -e "  ${GREEN}✓ $HEADER_COUNT header files copied${NC}"
echo ""

# Copy sample binaries
echo -e "${BLUE}[5/6] Copying sample binaries...${NC}"

# Plate detection samples
cp "$BUILD_DIR/bin/yolov11_plate_detector_trt_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true
cp "$BUILD_DIR/bin/plate_recognition_video_output_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true

# Face detection samples
cp "$BUILD_DIR/bin/yolov11_face_detector_trt_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true
cp "$BUILD_DIR/bin/yolov11_face_detector_video_output_sample" "$BUNDLE_DIR/bin/" 2>/dev/null || true

echo -e "${GREEN}✓ Sample binaries copied${NC}"
echo ""

# Copy models
echo -e "${BLUE}[6/6] Copying models...${NC}"

# TensorRT engines (if exist)
if [ -d "$TRT_MODELS_DIR/face" ]; then
    cp "$TRT_MODELS_DIR/face/"*.engine "$BUNDLE_DIR/models/trt/face/" 2>/dev/null || true
    echo -e "  ${GREEN}✓ Face TRT engines copied${NC}"
fi

if [ -d "${PWD}/cvedix_data/models/tensorrt" ]; then
    cp "${PWD}/cvedix_data/models/tensorrt/"*plate*.engine "$BUNDLE_DIR/models/trt/plate/" 2>/dev/null || true
    echo -e "  ${GREEN}✓ Plate TRT engines copied${NC}"
fi

# ONNX models (for conversion)
if [ -f "${PWD}/cvedix_data/models/face/face_detection_yolov11.onnx" ]; then
    cp "${PWD}/cvedix_data/models/face/face_detection_yolov11.onnx" "$BUNDLE_DIR/models/onnx/face/"
    cp "${PWD}/cvedix_data/models/face/face_detection_yolov11_fp16.onnx" "$BUNDLE_DIR/models/onnx/face/" 2>/dev/null || true
    echo -e "  ${GREEN}✓ Face ONNX models copied${NC}"
fi

if [ -f "${PWD}/cvedix_data/models/plate/license-plate-finetune-v1x.onnx" ]; then
    cp "${PWD}/cvedix_data/models/plate/"*.onnx "$BUNDLE_DIR/models/onnx/plate/" 2>/dev/null || true
    echo -e "  ${GREEN}✓ Plate ONNX models copied${NC}"
fi

# Copy sample test videos
if [ -f "${PWD}/cvedix_data/video/face.mp4" ]; then
    cp "${PWD}/cvedix_data/video/face.mp4" "$BUNDLE_DIR/video/"
    echo -e "  ${GREEN}✓ Test video (face.mp4) copied${NC}"
fi

if [ -f "${PWD}/cvedix_data/video/vietnam_plate.mp4" ]; then
    cp "${PWD}/cvedix_data/video/vietnam_plate.mp4" "$BUNDLE_DIR/video/"
    echo -e "  ${GREEN}✓ Test video (vietnam_plate.mp4) copied${NC}"
fi

echo ""

# Create README
cat > "$BUNDLE_DIR/README.md" << 'EOF'
# CVEDIX TensorRT Detection Bundle

## Nội dung

Bundle này bao gồm:
- **Plate Detection**: YOLOv11 TRT License Plate Detection
- **Face Detection**: YOLOv11 TRT Face Detection

## Cấu trúc thư mục

```
bundle_output/
├── lib/                          # Shared libraries
│   ├── libcvedix_core.so
│   ├── libtrt_yolov11.so
│   └── libtrt_yolov11_face.so
├── bin/                          # Sample binaries
│   ├── yolov11_plate_detector_trt_sample
│   ├── yolov11_face_detector_trt_sample
│   └── yolov11_face_detector_video_output_sample
├── models/
│   ├── trt/                      # TensorRT engines (ready to use)
│   │   ├── plate/
│   │   └── face/
│   └── onnx/                     # ONNX models (for conversion)
│       ├── plate/
│       └── face/
├── include/                      # Header files
└── video/                   # Test videos
```

## Yêu cầu

- NVIDIA GPU với CUDA support
- TensorRT >= 8.x (khuyến nghị 10.x)
- GStreamer 1.0

## Sử dụng

### 1. Convert ONNX to TensorRT (nếu chưa có .engine)

```bash
# Face detection
/usr/src/tensorrt/bin/trtexec \
    --onnx=./models/onnx/face/face_detection_yolov11.onnx \
    --saveEngine=./models/trt/face/yolov11_face_fp16.engine \
    --fp16 --memPoolSize=workspace:4096

# Plate detection
/usr/src/tensorrt/bin/trtexec \
    --onnx=./models/onnx/plate/license-plate-finetune-v1x.onnx \
    --saveEngine=./models/trt/plate/plate_detector_fp16.engine \
    --fp16 --memPoolSize=workspace:4096
```

### 2. Chạy samples

```bash
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH

# Face detection
./bin/yolov11_face_detector_trt_sample \
    ./models/trt/face/yolov11_face_fp16.engine \
    ./video/face.mp4

# Face detection với video output
./bin/yolov11_face_detector_video_output_sample \
    ./models/trt/face/yolov11_face_fp16.engine \
    ./video/face.mp4 \
    ./output

# Plate detection
./bin/yolov11_plate_detector_trt_sample \
    ./models/trt/plate/plate_detector_fp16.engine \
    ./video/vietnam_plate.mp4
```

## Performance

| Model | GPU | FPS |
|-------|-----|-----|
| YOLOv11 Face (FP16) | RTX 3080 | ~925 |
| YOLOv11 Plate (FP16) | RTX 3080 | ~900 |

## License

CVEDIX Proprietary License
EOF

# Create setup script
cat > "$BUNDLE_DIR/setup_env.sh" << 'EOF'
#!/bin/bash
# Setup environment for CVEDIX TRT Detection Bundle

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
export PATH="$SCRIPT_DIR/bin:$PATH"

echo "Environment configured:"
echo "  LD_LIBRARY_PATH includes: $SCRIPT_DIR/lib"
echo "  PATH includes: $SCRIPT_DIR/bin"
echo ""
echo "Available commands:"
echo "  - yolov11_face_detector_trt_sample"
echo "  - yolov11_face_detector_video_output_sample"
echo "  - yolov11_plate_detector_trt_sample"
EOF
chmod +x "$BUNDLE_DIR/setup_env.sh"

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
VERSION_EOF

echo -e "${GREEN}✓ VERSION file created${NC}"

# Create tarball
echo -e "${BLUE}Creating tarball...${NC}"
TARBALL="${BUNDLE_NAME}-${VERSION}-$(uname -m).tar.gz"
cd "$BUNDLE_DIR/.."
tar -czf "$TARBALL" -C "$(dirname $BUNDLE_DIR)" "$(basename $BUNDLE_DIR)"
mv "$TARBALL" "${PWD}/"

# Summary
echo ""
echo -e "${CYAN}===================================================${NC}"
echo -e "${GREEN}Bundle created successfully!${NC}"
echo -e "${CYAN}===================================================${NC}"
echo ""
echo -e "Bundle directory: ${YELLOW}$BUNDLE_DIR${NC}"
echo -e "Tarball: ${YELLOW}${PWD}/${TARBALL}${NC}"
echo ""

# Show bundle size
BUNDLE_SIZE=$(du -sh "$BUNDLE_DIR" | cut -f1)
TARBALL_SIZE=$(du -sh "${PWD}/${TARBALL}" | cut -f1)
echo -e "Bundle size: ${YELLOW}$BUNDLE_SIZE${NC}"
echo -e "Tarball size: ${YELLOW}$TARBALL_SIZE${NC}"
echo ""

# List contents
echo -e "${BLUE}Bundle contents:${NC}"
echo ""
echo "Libraries:"
ls -lh "$BUNDLE_DIR/lib/" 2>/dev/null | grep -v "^total" | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "Binaries:"
ls -lh "$BUNDLE_DIR/bin/" 2>/dev/null | grep -v "^total" | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "TRT Models:"
find "$BUNDLE_DIR/models/trt" -name "*.engine" 2>/dev/null | while read f; do
    size=$(du -h "$f" | cut -f1)
    echo "  $(basename $f) ($size)"
done
echo ""
echo "ONNX Models:"
find "$BUNDLE_DIR/models/onnx" -name "*.onnx" 2>/dev/null | while read f; do
    size=$(du -h "$f" | cut -f1)
    echo "  $(basename $f) ($size)"
done
echo ""

echo -e "${GREEN}Done!${NC}"
