#!/bin/bash
set -e

#########################################
# Build Debian Package for CPU (x86_64/amd64)
# Script automatically builds and packages SDK for CPU-only
#########################################

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  CVEDIX CPU Package Builder (x86_64)       ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

# Check architecture
ARCH=$(uname -m)
echo -e "${BLUE}Architecture: ${ARCH}${NC}"

# Configuration
BUILD_DIR="build_pkg_cpu"
BUILD_SAMPLES=${BUILD_SAMPLES:-ON}
BUILD_TYPE=${BUILD_TYPE:-Release}
CVEDIX_DATA_SOURCE=${CVEDIX_DATA_SOURCE:-"./build/bin/cvedix_data"}

echo -e "${BLUE}Configuration:${NC}"
echo -e "  Build type: ${YELLOW}${BUILD_TYPE}${NC}"
echo -e "  Build samples: ${YELLOW}${BUILD_SAMPLES}${NC}"
echo -e "  CPU Backend: OpenCV DNN (ONNX models)"
echo ""

# Clean previous build
if [ -d "$BUILD_DIR" ]; then
    echo -e "${BLUE}Cleaning previous build...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

# Prepare cvedix_data
echo -e "${BLUE}[1/4] Preparing cvedix_data...${NC}"
if [ -d "../cvedix_data" ]; then
    echo -e "${GREEN}✓ cvedix_data already exists in source directory${NC}"
    
    # Check for ONNX models
    ONNX_COUNT=$(find ../cvedix_data -name "*.onnx" 2>/dev/null | wc -l)
    RKNN_COUNT=$(find ../cvedix_data -name "*.rknn" 2>/dev/null | wc -l)
    
    if [ $ONNX_COUNT -gt 0 ]; then
        echo -e "${GREEN}✓ Found ${ONNX_COUNT} ONNX models${NC}"
    else
        echo -e "${YELLOW}⚠ Warning: No ONNX models found${NC}"
    fi
    
    if [ $RKNN_COUNT -gt 0 ]; then
        echo -e "${YELLOW}! Note: Found ${RKNN_COUNT} RKNN models (will be included but not usable on CPU)${NC}"
    fi
elif [ -d "../${CVEDIX_DATA_SOURCE}" ]; then
    echo -e "${YELLOW}! cvedix_data not in source, creating symlink from ${CVEDIX_DATA_SOURCE}${NC}"
    ln -sf "../${CVEDIX_DATA_SOURCE}" ../cvedix_data
    echo -e "${GREEN}✓ cvedix_data symlink created${NC}"
else
    echo -e "${YELLOW}⚠ Warning: cvedix_data not found${NC}"
    echo -e "${YELLOW}  Package will be created without cvedix_data (models, test videos)${NC}"
    echo -e "${YELLOW}  To include cvedix_data with ONNX models, run:${NC}"
    echo -e "${YELLOW}    ./build_cpu_package_with_models.sh${NC}"
fi

# Configure CMake for CPU
echo ""
echo -e "${BLUE}[2/4] Configuring CMake for CPU...${NC}"
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_TRT=OFF \
    -DCVEDIX_WITH_RKNN=OFF \
    -DCVEDIX_WITH_RGA=OFF \
    -DCVEDIX_WITH_FFMPEG=OFF \
    -DCVEDIX_WITH_LLM=OFF \
    -DCVEDIX_WITH_KAFKA=OFF \
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
echo -e "${BLUE}[3/4] Building SDK...${NC}"
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build completed${NC}"

# Create package
echo ""
echo -e "${BLUE}[4/4] Creating Debian package...${NC}"
make package

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
    echo -e "Location:     ${YELLOW}$(pwd)/${NC}"
    echo ""
    
    echo -e "${BLUE}Package info:${NC}"
    dpkg -I "$DEB_FILE" | grep -E "Package|Version|Architecture|Depends" | sed 's/^/ /'
    echo ""
    
    echo -e "${BLUE}Models included:${NC}"
    ONNX_IN_PKG=$(dpkg -c "$DEB_FILE" 2>/dev/null | grep -c "\.onnx" || echo "0")
    echo -e "  ONNX models: ${YELLOW}${ONNX_IN_PKG}${NC}"
    echo ""
    
    echo -e "${BLUE}To install:${NC}"
    echo -e "  ${YELLOW}sudo apt-get install ./${DEB_FILE}${NC}"
    echo ""
    
    echo -e "${BLUE}To check contents:${NC}"
    echo -e "  ${YELLOW}dpkg -c ${DEB_FILE}${NC}"
    echo ""
    
    echo -e "${BLUE}To list ONNX models in package:${NC}"
    echo -e "  ${YELLOW}dpkg -c ${DEB_FILE} | grep .onnx${NC}"
    echo ""
else
    echo -e "${RED}Error: Package file not found!${NC}"
    exit 1
fi

echo -e "${GREEN}✅ All done!${NC}"
echo ""
echo -e "${BLUE}Note:${NC}"
echo -e "  This package uses CPU-only inference with OpenCV DNN backend."
echo -e "  Supported models: ONNX format (.onnx files)"
echo -e "  No RKNN, TensorRT, or FFmpeg support included."
echo ""
