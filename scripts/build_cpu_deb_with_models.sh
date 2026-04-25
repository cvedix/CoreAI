#!/bin/bash
set -e

#########################################
# Build Debian Package for CPU with ONNX Models
# Script automatically prepares models and builds Debian package
#########################################

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  CVEDIX CPU Debian Package Builder         ${NC}"
echo -e "${GREEN}  (with ONNX Models)                        ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

# Parse arguments
MODEL_OPTION="2"  # Default: all ONNX models
BUILD_SAMPLES=${BUILD_SAMPLES:-ON}
BUILD_TYPE=${BUILD_TYPE:-Release}
SKIP_MODEL_PREP=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --model-option=*)
            MODEL_OPTION="${1#*=}"
            shift
            ;;
        --skip-model-prep)
            SKIP_MODEL_PREP=true
            shift
            ;;
        --build-samples=*)
            BUILD_SAMPLES="${1#*=}"
            shift
            ;;
        --build-type=*)
            BUILD_TYPE="${1#*=}"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --model-option=N     Model selection (1=all, 2=ONNX only, 3=minimal)"
            echo "                       Default: 2 (ONNX models only)"
            echo "  --skip-model-prep    Skip model preparation (use existing cvedix_data)"
            echo "  --build-samples=ON|OFF  Build samples (default: ON)"
            echo "  --build-type=TYPE    Build type: Debug or Release (default: Release)"
            echo "  --help, -h           Show this help"
            echo ""
            echo "Model Options:"
            echo "  1) All data (~3GB) - Includes all models, videos, test data"
            echo "  2) ONNX models only (~800MB) - Recommended for CPU packaging"
            echo "  3) Minimal ONNX models (~200MB) - Face detection + YOLOv11 only"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check architecture
ARCH=$(uname -m)
echo -e "${BLUE}Architecture: ${ARCH}${NC}"

# Configuration
BUILD_DIR="build_pkg_cpu"
CVEDIX_DATA_SOURCE=${CVEDIX_DATA_SOURCE:-"./build/bin/cvedix_data"}

echo -e "${BLUE}Configuration:${NC}"
echo -e "  Build type: ${YELLOW}${BUILD_TYPE}${NC}"
echo -e "  Build samples: ${YELLOW}${BUILD_SAMPLES}${NC}"
echo -e "  Model option: ${YELLOW}${MODEL_OPTION}${NC}"
echo -e "  CPU Backend: OpenCV DNN (ONNX models)"
echo ""

# Step 1: Prepare cvedix_data with models
if [ "$SKIP_MODEL_PREP" = false ]; then
    echo -e "${BLUE}[1/5] Preparing cvedix_data with models...${NC}"
    
    if [ -d "cvedix_data" ]; then
        echo -e "${YELLOW}⚠ Warning: cvedix_data already exists${NC}"
        read -p "Overwrite existing cvedix_data? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -rf cvedix_data
        else
            echo -e "${BLUE}Using existing cvedix_data${NC}"
            SKIP_MODEL_PREP=true
        fi
    fi
    
    if [ "$SKIP_MODEL_PREP" = false ]; then
        if [ -d "${CVEDIX_DATA_SOURCE}" ]; then
            case $MODEL_OPTION in
                1)
                    echo -e "${BLUE}Copying all cvedix_data (~3GB)...${NC}"
                    echo "This may take a few minutes..."
                    cp -r "${CVEDIX_DATA_SOURCE}" ./cvedix_data
                    echo -e "${GREEN}✓ Copied all data${NC}"
                    ;;
                2)
                    echo -e "${BLUE}Creating cvedix_data with all ONNX models (~800MB)...${NC}"
                    mkdir -p cvedix_data/models
                    mkdir -p cvedix_data/video
                    
                    # Copy all ONNX models recursively
                    echo "  Copying ONNX models..."
                    if [ -d "${CVEDIX_DATA_SOURCE}/models" ]; then
                        find "${CVEDIX_DATA_SOURCE}/models" -name "*.onnx" -type f | while read file; do
                            rel_path="${file#${CVEDIX_DATA_SOURCE}/models/}"
                            target_dir="cvedix_data/models/$(dirname "$rel_path")"
                            mkdir -p "$target_dir"
                            cp "$file" "$target_dir/"
                            echo -n "."
                        done
                        echo ""
                    fi
                    
                    # Copy labels files (small, important for models)
                    echo "  Copying labels and config files..."
                    if [ -d "${CVEDIX_DATA_SOURCE}/models" ]; then
                        find "${CVEDIX_DATA_SOURCE}/models" \( -name "*.txt" -o -name "*.json" -o -name "*.yaml" -o -name "*.yml" \) -type f | while read file; do
                            rel_path="${file#${CVEDIX_DATA_SOURCE}/models/}"
                            target_dir="cvedix_data/models/$(dirname "$rel_path")"
                            mkdir -p "$target_dir"
                            cp "$file" "$target_dir/"
                        done
                    fi
                    
                    # Copy test videos (optional, for testing)
                    echo "  Copying test videos..."
                    if [ -d "${CVEDIX_DATA_SOURCE}/video" ]; then
                        find "${CVEDIX_DATA_SOURCE}/video" -name "*.mp4" -o -name "*.avi" -o -name "*.mkv" | head -10 | while read file; do
                            cp "$file" ./cvedix_data/video/ 2>/dev/null || true
                        done
                    fi
                    
                    echo -e "${GREEN}✓ Created cvedix_data with ONNX models${NC}"
                    ;;
                3)
                    echo -e "${BLUE}Creating minimal cvedix_data with essential ONNX models (~200MB)...${NC}"
                    mkdir -p cvedix_data/models/face
                    mkdir -p cvedix_data/models/face/face_recognition
                    mkdir -p cvedix_data/models
                    mkdir -p cvedix_data/video
                    
                    # Copy essential face detection models
                    echo "  Copying face detection models..."
                    if [ -f "${CVEDIX_DATA_SOURCE}/models/face/face_detection_yunet_2023mar.onnx" ]; then
                        cp "${CVEDIX_DATA_SOURCE}/models/face/face_detection_yunet_2023mar.onnx" \
                           ./cvedix_data/models/face/ 2>/dev/null || true
                    fi
                    if [ -f "${CVEDIX_DATA_SOURCE}/models/face/face_detection_yolov11.onnx" ]; then
                        cp "${CVEDIX_DATA_SOURCE}/models/face/face_detection_yolov11.onnx" \
                           ./cvedix_data/models/face/ 2>/dev/null || true
                    fi
                    
                    # Copy face recognition models
                    echo "  Copying face recognition models..."
                    if [ -d "${CVEDIX_DATA_SOURCE}/models/face/face_recognition" ]; then
                        find "${CVEDIX_DATA_SOURCE}/models/face/face_recognition" -name "*.onnx" | while read file; do
                            cp "$file" ./cvedix_data/models/face/face_recognition/ 2>/dev/null || true
                        done
                    fi
                    
                    # Copy YOLO models (if available)
                    if [ -d "${CVEDIX_DATA_SOURCE}/models/yolo" ]; then
                        mkdir -p ./cvedix_data/models/yolo
                        find "${CVEDIX_DATA_SOURCE}/models/yolo" -name "yolov11*.onnx" | head -3 | while read file; do
                            cp "$file" ./cvedix_data/models/yolo/ 2>/dev/null || true
                        done
                    fi
                    
                    # Copy labels
                    echo "  Copying labels..."
                    if [ -f "${CVEDIX_DATA_SOURCE}/models/coco_80classes.txt" ]; then
                        cp "${CVEDIX_DATA_SOURCE}/models/coco_80classes.txt" \
                           ./cvedix_data/models/ 2>/dev/null || true
                    fi
                    
                    # Copy one test video
                    echo "  Copying test video..."
                    if [ -f "${CVEDIX_DATA_SOURCE}/video/face.mp4" ]; then
                        cp "${CVEDIX_DATA_SOURCE}/video/face.mp4" \
                           ./cvedix_data/video/ 2>/dev/null || true
                    fi
                    
                    echo -e "${GREEN}✓ Created minimal cvedix_data${NC}"
                    ;;
                *)
                    echo -e "${RED}Error: Invalid model option: ${MODEL_OPTION}${NC}"
                    echo "Use 1, 2, or 3. See --help for details"
                    exit 1
                    ;;
            esac
        else
            echo -e "${RED}Error: cvedix_data source not found at: ${CVEDIX_DATA_SOURCE}${NC}"
            echo -e "${YELLOW}Please ensure you have built the project first:${NC}"
            echo -e "  mkdir -p build && cd build"
            echo -e "  cmake -DCVEDIX_WITH_GSTREAMER=ON \\"
            echo -e "        -DCVEDIX_BUILD_SAMPLES=ON .."
            echo -e "  make -j\$(nproc)"
            exit 1
        fi
    fi
else
    echo -e "${BLUE}[1/5] Skipping model preparation (using existing cvedix_data)${NC}"
fi

# Show cvedix_data info
if [ -d "cvedix_data" ]; then
    SIZE=$(du -sh cvedix_data 2>/dev/null | cut -f1)
    ONNX_COUNT=$(find cvedix_data -name "*.onnx" 2>/dev/null | wc -l)
    VIDEO_COUNT=$(find cvedix_data/video -type f 2>/dev/null | wc -l)
    
    echo -e "${GREEN}✓ cvedix_data ready${NC}"
    echo -e "  Size: ${YELLOW}${SIZE}${NC}"
    echo -e "  ONNX models: ${YELLOW}${ONNX_COUNT}${NC}"
    if [ $VIDEO_COUNT -gt 0 ]; then
        echo -e "  Test videos: ${YELLOW}${VIDEO_COUNT}${NC}"
    fi
    echo ""
else
    echo -e "${YELLOW}⚠ Warning: cvedix_data not found - package will be created without models${NC}"
    echo ""
fi

# Clean previous build
if [ -d "$BUILD_DIR" ]; then
    echo -e "${BLUE}[2/5] Cleaning previous build...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

# Configure CMake for CPU
echo -e "${BLUE}[3/5] Configuring CMake for CPU...${NC}"
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_TRT=OFF \
    -DCVEDIX_WITH_RKNN=OFF \
    -DCVEDIX_WITH_RGA=OFF \
    -DCVEDIX_WITH_LLM=OFF \
    -DCVEDIX_WITH_KAFKA=OFF \
    -DCVEDIX_WITH_PADDLE=OFF \
    -DCVEDIX_BUILD_SAMPLES=${BUILD_SAMPLES} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    ..

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ CMake configured${NC}"

# Build
echo ""
echo -e "${BLUE}[4/5] Building SDK...${NC}"
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build completed${NC}"

# Create Debian package
echo ""
echo -e "${BLUE}[5/5] Creating Debian package...${NC}"
cpack -G DEB

if [ $? -ne 0 ]; then
    echo -e "${RED}Package creation failed!${NC}"
    exit 1
fi

# Show results
echo ""
echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  Package Build Completed!                  ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

DEB_FILE=$(ls cvedix-ai-runtime*.deb 2>/dev/null | head -1)
if [ -n "$DEB_FILE" ]; then
    SIZE=$(du -h "$DEB_FILE" | cut -f1)
    echo -e "Package file: ${YELLOW}${DEB_FILE}${NC}"
    echo -e "Size:         ${YELLOW}${SIZE}${NC}"
    echo -e "Location:     ${YELLOW}$(pwd)/${DEB_FILE}${NC}"
    echo ""
    
    echo -e "${BLUE}Package info:${NC}"
    dpkg -I "$DEB_FILE" | grep -E "Package|Version|Architecture|Depends|Installed-Size" | sed 's/^/ /'
    echo ""
    
    echo -e "${BLUE}Models included:${NC}"
    ONNX_IN_PKG=$(dpkg -c "$DEB_FILE" 2>/dev/null | grep -c "\.onnx" || echo "0")
    VIDEO_IN_PKG=$(dpkg -c "$DEB_FILE" 2>/dev/null | grep -c "video.*\.\(mp4\|avi\|mkv\)" || echo "0")
    echo -e "  ONNX models: ${YELLOW}${ONNX_IN_PKG}${NC}"
    if [ $VIDEO_IN_PKG -gt 0 ]; then
        echo -e "  Test videos: ${YELLOW}${VIDEO_IN_PKG}${NC}"
    fi
    echo ""
    
    # Show some model paths
    if [ $ONNX_IN_PKG -gt 0 ]; then
        echo -e "${BLUE}Sample model paths in package:${NC}"
        dpkg -c "$DEB_FILE" 2>/dev/null | grep "\.onnx" | head -5 | awk '{print "  " $6}' | sed 's|opt/cvedix/bin/||'
        if [ $ONNX_IN_PKG -gt 5 ]; then
            echo -e "  ... and $((ONNX_IN_PKG - 5)) more"
        fi
        echo ""
    fi
    
    echo -e "${BLUE}Installation:${NC}"
    echo -e "  ${YELLOW}sudo apt-get install ./${DEB_FILE}${NC}"
    echo ""
    
    echo -e "${BLUE}After installation, models will be at:${NC}"
    echo -e "  ${YELLOW}/opt/cvedix/bin/cvedix_data/models/${NC}"
    echo ""
    
    echo -e "${BLUE}Useful commands:${NC}"
    echo -e "  View package contents:  ${YELLOW}dpkg -c ${DEB_FILE}${NC}"
    echo -e "  List ONNX models:       ${YELLOW}dpkg -c ${DEB_FILE} | grep .onnx${NC}"
    echo -e "  Extract package:        ${YELLOW}dpkg-deb -x ${DEB_FILE} ./extracted${NC}"
    echo ""
else
    echo -e "${RED}Error: Package file not found!${NC}"
    exit 1
fi

echo -e "${GREEN}✅ All done!${NC}"
echo ""
echo -e "${BLUE}Notes:${NC}"
echo -e "  • This package uses CPU-only inference with OpenCV DNN backend"
echo -e "  • Supported models: ONNX format (.onnx files)"
echo -e "  • No RKNN or TensorRT support included"
echo -e "  • Models are located at: /opt/cvedix/bin/cvedix_data/models/"
echo ""

