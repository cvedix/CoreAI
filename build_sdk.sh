#!/bin/bash
set -e

####################################
# Script để build và đóng gói SDK C++
# Usage: ./build_sdk.sh [--prefix=/path/to/install] [--build-type=Release|Debug]
####################################

# Default values
INSTALL_PREFIX=$(pwd)/output
BUILD_TYPE=Release
BUILD_DIR=build_sdk
ENABLE_CUDA=OFF
ENABLE_TRT=OFF
ENABLE_PADDLE=OFF
ENABLE_KAFKA=OFF
ENABLE_LLM=OFF
ENABLE_FFMPEG=OFF
ENABLE_RKNN=OFF
ENABLE_RGA=OFF
BUILD_SAMPLES=OFF

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --prefix=*)
            INSTALL_PREFIX="${1#*=}"
            shift
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --build-type=*)
            BUILD_TYPE="${1#*=}"
            shift
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --with-cuda)
            ENABLE_CUDA=ON
            shift
            ;;
        --with-trt)
            ENABLE_TRT=ON
            shift
            ;;
        --with-paddle)
            ENABLE_PADDLE=ON
            shift
            ;;
        --with-kafka)
            ENABLE_KAFKA=ON
            shift
            ;;
        --with-llm)
            ENABLE_LLM=ON
            shift
            ;;
        --with-ffmpeg)
            ENABLE_FFMPEG=ON
            shift
            ;;
        --with-rknn)
            ENABLE_RKNN=ON
            shift
            ;;
        --with-rga)
            ENABLE_RGA=ON
            shift
            ;;
        --build-samples)
            BUILD_SAMPLES=ON
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --prefix=PATH          Installation prefix (default: ./output)"
            echo "  --build-type=TYPE      Build type: Release or Debug (default: Release)"
            echo "  --with-cuda            Enable CUDA support"
            echo "  --with-trt             Enable TensorRT support"
            echo "  --with-paddle          Enable PaddlePaddle support"
            echo "  --with-kafka           Enable Kafka support"
            echo "  --with-llm             Enable LLM support"
            echo "  --with-ffmpeg          Enable FFmpeg support"
            echo "  --with-rknn             Enable RKNN support"
            echo "  --with-rga              Enable RGA support"
            echo "  --build-samples         Build sample applications"
            echo "  --help                 Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  CVEDIX Instance Pipeline SDK Builder${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "Install prefix: ${YELLOW}$INSTALL_PREFIX${NC}"
echo -e "Build type: ${YELLOW}$BUILD_TYPE${NC}"
echo -e "Build directory: ${YELLOW}$BUILD_DIR${NC}"
echo ""
echo -e "Features:"
echo -e "  CUDA: ${YELLOW}$ENABLE_CUDA${NC}"
echo -e "  TensorRT: ${YELLOW}$ENABLE_TRT${NC}"
echo -e "  PaddlePaddle: ${YELLOW}$ENABLE_PADDLE${NC}"
echo -e "  Kafka: ${YELLOW}$ENABLE_KAFKA${NC}"
echo -e "  LLM: ${YELLOW}$ENABLE_LLM${NC}"
echo -e "  FFmpeg: ${YELLOW}$ENABLE_FFMPEG${NC}"
echo -e "  RKNN: ${YELLOW}$ENABLE_RKNN${NC}"
echo -e "  RGA: ${YELLOW}$ENABLE_RGA${NC}"
echo -e "  Build Samples: ${YELLOW}$BUILD_SAMPLES${NC}"
echo ""

# Create build directory
echo -e "${GREEN}[1/4] Creating build directory...${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
echo -e "${GREEN}[2/4] Configuring CMake...${NC}"
cmake .. \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCVEDIX_WITH_CUDA="$ENABLE_CUDA" \
    -DCVEDIX_WITH_TRT="$ENABLE_TRT" \
    -DCVEDIX_WITH_PADDLE="$ENABLE_PADDLE" \
    -DCVEDIX_WITH_KAFKA="$ENABLE_KAFKA" \
    -DCVEDIX_WITH_LLM="$ENABLE_LLM" \
    -DCVEDIX_WITH_FFMPEG="$ENABLE_FFMPEG" \
    -DCVEDIX_WITH_RKNN="$ENABLE_RKNN" \
    -DCVEDIX_WITH_RGA="$ENABLE_RGA" \
    -DCVEDIX_BUILD_COMPLEX_SAMPLES="$BUILD_SAMPLES"

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

# Build
echo -e "${GREEN}[3/4] Building SDK...${NC}"
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# Install
echo -e "${GREEN}[4/4] Installing SDK to $INSTALL_PREFIX...${NC}"
make install

if [ $? -ne 0 ]; then
    echo -e "${RED}Installation failed!${NC}"
    exit 1
fi

# Create SDK package structure
echo -e "${GREEN}[5/5] Creating SDK package structure...${NC}"
cd ..

# Create additional SDK files
SDK_ROOT="$INSTALL_PREFIX"
mkdir -p "$SDK_ROOT/share/cvedix"

# Create pkg-config file
cat > "$SDK_ROOT/lib/pkgconfig/cvedix.pc" << EOF
prefix=$INSTALL_PREFIX
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: cvedix
Description: CVEDIX Instance Pipeline SDK
Version: 1.0
Libs: -L\${libdir} -lcvedix_instance_sdk
Cflags: -I\${includedir}
Requires: opencv4
EOF

# Create SDK info file
cat > "$SDK_ROOT/share/cvedix/sdk_info.txt" << EOF
CVEDIX Instance Pipeline SDK
============================
Version: 1.0
Build Type: $BUILD_TYPE
Install Prefix: $INSTALL_PREFIX

Features Enabled:
- CUDA: $ENABLE_CUDA
- TensorRT: $ENABLE_TRT
- PaddlePaddle: $ENABLE_PADDLE
- Kafka: $ENABLE_KAFKA
- LLM: $ENABLE_LLM
- FFmpeg: $ENABLE_FFMPEG
- RKNN: $ENABLE_RKNN
- RGA: $ENABLE_RGA

Installation Date: $(date)
EOF

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  SDK built and installed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "SDK location: ${YELLOW}$INSTALL_PREFIX${NC}"
echo ""
echo -e "Directory structure:"
echo -e "  ${YELLOW}$INSTALL_PREFIX/lib${NC}          - Libraries"
echo -e "  ${YELLOW}$INSTALL_PREFIX/include/cvedix${NC} - Header files"
echo -e "  ${YELLOW}$INSTALL_PREFIX/lib/cmake/cvedix${NC} - CMake config files"
echo -e "  ${YELLOW}$INSTALL_PREFIX/lib/pkgconfig${NC} - pkg-config files"
echo ""
echo -e "To use the SDK in your project, see ${YELLOW}README_SDK.md${NC}"
echo ""

