#!/bin/bash

####################################
# Build OpenCV with freetype module
# 
# This script builds OpenCV with the freetype contrib module
# which is required for Chinese font support in OSD nodes.
#
# Prerequisites:
#   - OpenCV source code
#   - opencv_contrib source code
#   - Required dependencies (cmake, build-essential, etc.)
#
# Usage:
#   ./scripts/build_opencv_with_freetype.sh
####################################

set -e  # Exit on error

# Configuration
OPENCV_SRC_DIR="${OPENCV_SRC_DIR:-/home/ubuntu/opencv}"
OPENCV_CONTRIB_DIR="${OPENCV_CONTRIB_DIR:-/home/ubuntu/opencv_contrib}"
OPENCV_BUILD_DIR="${OPENCV_BUILD_DIR:-/home/ubuntu/opencv_build}"
OPENCV_INSTALL_PREFIX="${OPENCV_INSTALL_PREFIX:-/usr/local}"

echo "=========================================="
echo "Building OpenCV with freetype module"
echo "=========================================="
echo "OpenCV source:      $OPENCV_SRC_DIR"
echo "OpenCV contrib:     $OPENCV_CONTRIB_DIR"
echo "Build directory:    $OPENCV_BUILD_DIR"
echo "Install prefix:     $OPENCV_INSTALL_PREFIX"
echo "=========================================="
echo ""

# Check if source directories exist
if [ ! -d "$OPENCV_SRC_DIR" ]; then
    echo "❌ Error: OpenCV source directory not found: $OPENCV_SRC_DIR"
    echo "   Please set OPENCV_SRC_DIR environment variable or clone OpenCV first"
    exit 1
fi

if [ ! -d "$OPENCV_CONTRIB_DIR" ]; then
    echo "❌ Error: OpenCV contrib directory not found: $OPENCV_CONTRIB_DIR"
    echo "   Please set OPENCV_CONTRIB_DIR environment variable or clone opencv_contrib first"
    exit 1
fi

# Create build directory
if [ -d "$OPENCV_BUILD_DIR" ]; then
    echo "⚠️  Build directory '$OPENCV_BUILD_DIR' already exists."
    read -p "Do you want to remove it and start fresh? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🧹 Removing existing build directory..."
        rm -rf "$OPENCV_BUILD_DIR"
    else
        echo "📦 Using existing build directory..."
    fi
fi

mkdir -p "$OPENCV_BUILD_DIR"
cd "$OPENCV_BUILD_DIR"

# Configure CMake
echo ""
echo "🔧 Configuring CMake..."
cmake \
    -D CMAKE_BUILD_TYPE=RELEASE \
    -D CMAKE_INSTALL_PREFIX="$OPENCV_INSTALL_PREFIX" \
    -D OPENCV_EXTRA_MODULES_PATH="$OPENCV_CONTRIB_DIR/modules" \
    -D BUILD_EXAMPLES=ON \
    -D WITH_FREETYPE=ON \
    "$OPENCV_SRC_DIR"

# Build
echo ""
echo "🔨 Building OpenCV (this may take a while)..."
make -j$(nproc)

# Install (optional, requires sudo)
echo ""
read -p "Do you want to install OpenCV to $OPENCV_INSTALL_PREFIX? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "📦 Installing OpenCV..."
    sudo make install
    sudo ldconfig
    echo "✅ OpenCV installed successfully!"
else
    echo "⏭️  Skipping installation. You can install later with:"
    echo "   cd $OPENCV_BUILD_DIR && sudo make install && sudo ldconfig"
fi

echo ""
echo "✅ Build completed successfully!"
echo ""
echo "OpenCV with freetype module is ready."
echo "Make sure to update your CMakeLists.txt or environment variables"
echo "to use the newly built OpenCV."

