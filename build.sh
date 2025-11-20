#!/bin/bash

####################################
# Build script for instance_pipeline
# 
# Usage:
#   ./build.sh                    # Build with default options
#   ./build.sh --cuda --trt       # Build with CUDA and TensorRT
#   ./build.sh --all              # Build with all optional features
####################################

set -e  # Exit on error

# Default build options
BUILD_TYPE="Debug"
BUILD_DIR="build"
WITH_CUDA=OFF
WITH_TRT=OFF
WITH_PADDLE=OFF
WITH_KAFKA=OFF
WITH_LLM=OFF
WITH_FFMPEG=OFF
WITH_RKNN=OFF
WITH_RGA=OFF
BUILD_COMPLEX_SAMPLES=OFF

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --cuda)
            WITH_CUDA=ON
            shift
            ;;
        --trt)
            WITH_TRT=ON
            shift
            ;;
        --paddle)
            WITH_PADDLE=ON
            shift
            ;;
        --kafka)
            WITH_KAFKA=ON
            shift
            ;;
        --llm)
            WITH_LLM=ON
            shift
            ;;
        --ffmpeg)
            WITH_FFMPEG=ON
            shift
            ;;
        --rknn)
            WITH_RKNN=ON
            shift
            ;;
        --rga)
            WITH_RGA=ON
            shift
            ;;
        --complex-samples)
            BUILD_COMPLEX_SAMPLES=ON
            shift
            ;;
        --all)
            WITH_CUDA=ON
            WITH_TRT=ON
            WITH_PADDLE=ON
            WITH_KAFKA=ON
            WITH_LLM=ON
            WITH_FFMPEG=ON
            WITH_RKNN=ON
            WITH_RGA=ON
            BUILD_COMPLEX_SAMPLES=ON
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --release           Build in Release mode (default: Debug)"
            echo "  --cuda              Enable CUDA support"
            echo "  --trt               Enable TensorRT support"
            echo "  --paddle            Enable PaddlePaddle support"
            echo "  --kafka             Enable Kafka support"
            echo "  --llm               Enable LLM support"
            echo "  --ffmpeg            Enable FFmpeg support"
            echo "  --rknn              Enable RKNN support"
            echo "  --rga               Enable RGA support"
            echo "  --complex-samples   Build complex samples"
            echo "  --all               Enable all optional features"
            echo "  --help, -h          Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "Building instance_pipeline"
echo "=========================================="
echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo ""
echo "Optional features:"
echo "  CUDA:              $WITH_CUDA"
echo "  TensorRT:          $WITH_TRT"
echo "  PaddlePaddle:      $WITH_PADDLE"
echo "  Kafka:             $WITH_KAFKA"
echo "  LLM:               $WITH_LLM"
echo "  FFmpeg:            $WITH_FFMPEG"
echo "  RKNN:              $WITH_RKNN"
echo "  RGA:               $WITH_RGA"
echo "  Complex Samples:   $BUILD_COMPLEX_SAMPLES"
echo "=========================================="
echo ""

# Create build directory
if [ -d "$BUILD_DIR" ]; then
    echo "⚠️  Build directory '$BUILD_DIR' already exists."
    read -p "Do you want to remove it and start fresh? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🧹 Removing existing build directory..."
        rm -rf "$BUILD_DIR"
    else
        echo "📦 Using existing build directory..."
    fi
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
echo ""
echo "🔧 Configuring CMake..."
cmake \
    -D CMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -D CVEDIX_WITH_CUDA="$WITH_CUDA" \
    -D CVEDIX_WITH_TRT="$WITH_TRT" \
    -D CVEDIX_WITH_PADDLE="$WITH_PADDLE" \
    -D CVEDIX_WITH_KAFKA="$WITH_KAFKA" \
    -D CVEDIX_WITH_LLM="$WITH_LLM" \
    -D CVEDIX_WITH_FFMPEG="$WITH_FFMPEG" \
    -D CVEDIX_WITH_RKNN="$WITH_RKNN" \
    -D CVEDIX_WITH_RGA="$WITH_RGA" \
    -D CVEDIX_BUILD_COMPLEX_SAMPLES="$BUILD_COMPLEX_SAMPLES" \
    ..

# Build
echo ""
echo "🔨 Building project..."
make -j$(nproc)

echo ""
echo "✅ Build completed successfully!"
echo ""
echo "Libraries are in: $BUILD_DIR/libs"
echo "Samples are in: $BUILD_DIR/samples"

