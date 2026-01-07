#!/bin/bash
set -e

#########################################
# Build Debian Package cho Rockchip ARM64
# Script tự động build và đóng gói SDK
#########################################

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  CVEDIX Package Builder for Rockchip       ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

# Check architecture
ARCH=$(uname -m)
if [ "$ARCH" != "aarch64" ]; then
    echo -e "${RED}Error: This script is for ARM64/aarch64 architecture${NC}"
    echo -e "${RED}Current architecture: ${ARCH}${NC}"
    exit 1
fi
echo -e "${BLUE}✓ Architecture: ${ARCH} (ARM64)${NC}"

# Configuration
BUILD_DIR="build_pkg"
BUILD_SAMPLES=${BUILD_SAMPLES:-OFF}
BUILD_TYPE=${BUILD_TYPE:-Release}
CVEDIX_DATA_SOURCE=${CVEDIX_DATA_SOURCE:-"./build/bin/cvedix_data"}

echo -e "${BLUE}Configuration:${NC}"
echo -e "  Build type: ${YELLOW}${BUILD_TYPE}${NC}"
echo -e "  Build samples: ${YELLOW}${BUILD_SAMPLES}${NC}"
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
elif [ -d "../${CVEDIX_DATA_SOURCE}" ]; then
    echo -e "${YELLOW}! cvedix_data not in source, creating symlink from ${CVEDIX_DATA_SOURCE}${NC}"
    ln -sf "../${CVEDIX_DATA_SOURCE}" ../cvedix_data
    echo -e "${GREEN}✓ cvedix_data symlink created${NC}"
else
    echo -e "${YELLOW}⚠ Warning: cvedix_data not found${NC}"
    echo -e "${YELLOW}  Package will be created without cvedix_data (models, test videos)${NC}"
    echo -e "${YELLOW}  To include cvedix_data, copy it to source directory:${NC}"
    echo -e "${YELLOW}    cp -r ./build/bin/cvedix_data ./cvedix_data${NC}"
fi

# Configure CMake
echo ""
echo -e "${BLUE}[2/4] Configuring CMake for Rockchip...${NC}"
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_WITH_RGA=ON \
    -DCVEDIX_WITH_FFMPEG=OFF \
    -DCVEDIX_WITH_LLM=OFF \
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
    
    echo -e "${BLUE}To install:${NC}"
    echo -e "  ${YELLOW}sudo apt-get install ./${DEB_FILE}${NC}"
    echo ""
    
    echo -e "${BLUE}To check contents:${NC}"
    echo -e "  ${YELLOW}dpkg -c ${DEB_FILE}${NC}"
    echo ""
else
    echo -e "${RED}Error: Package file not found!${NC}"
    exit 1
fi

echo -e "${GREEN}✅ All done!${NC}"

