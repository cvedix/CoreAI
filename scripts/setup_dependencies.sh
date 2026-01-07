#!/bin/bash
#
# Setup Dependencies Script for AI Core Runtime
# Auto-detects hardware and installs appropriate dependencies
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}==========================================${NC}"
echo -e "${BLUE} AI Core Runtime - Dependency Installer ${NC}"
echo -e "${BLUE}==========================================${NC}"

# Detect architecture and hardware
ARCH=$(uname -m)
echo -e "${GREEN}Detected architecture: ${ARCH}${NC}"

# Function to detect hardware platform
detect_platform() {
    if [ -f /proc/device-tree/compatible ]; then
        COMPATIBLE=$(cat /proc/device-tree/compatible 2>/dev/null | tr '\0' '\n' | head -1)
        if echo "$COMPATIBLE" | grep -qi "rockchip"; then
            echo "rockchip"
            return
        elif echo "$COMPATIBLE" | grep -qi "nvidia"; then
            echo "jetson"
            return
        fi
    fi
    
    # Check for NVIDIA GPU on x86_64
    if [ "$ARCH" = "x86_64" ]; then
        if command -v nvidia-smi &> /dev/null; then
            echo "nvidia_gpu"
            return
        fi
    fi
    
    echo "cpu_only"
}

PLATFORM=$(detect_platform)
echo -e "${GREEN}Detected platform: ${PLATFORM}${NC}"

# Function to install base dependencies
install_base_deps() {
    echo -e "${YELLOW}Installing base dependencies...${NC}"
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config \
        libopencv-dev \
        libeigen3-dev \
        libssl-dev
}

# Function to install GStreamer
install_gstreamer() {
    echo -e "${YELLOW}Installing GStreamer...${NC}"
    sudo apt-get install -y \
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
        libgstreamer-plugins-good1.0-dev \
        libgstreamer-plugins-bad1.0-dev \
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad \
        gstreamer1.0-plugins-ugly \
        gstreamer1.0-libav \
        gstreamer1.0-tools \
        gstreamer1.0-rtsp \
        libgstrtspserver-1.0-dev
}

# Function to install Rockchip-specific dependencies
install_rockchip_deps() {
    echo -e "${YELLOW}Installing Rockchip-specific dependencies...${NC}"
    
    # Check if RKNN toolkit is in third_party
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    if [ -d "$SCRIPT_DIR/third_party/rknn" ]; then
        echo -e "${GREEN}RKNN libraries found in third_party/rknn${NC}"
    else
        echo -e "${YELLOW}RKNN libraries not found in third_party/rknn${NC}"
        echo -e "${YELLOW}Please download RKNN toolkit from Rockchip official repository${NC}"
    fi
    
    # Check if RGA is in third_party
    if [ -d "$SCRIPT_DIR/third_party/librga" ]; then
        echo -e "${GREEN}RGA libraries found in third_party/librga${NC}"
    else
        echo -e "${YELLOW}RGA libraries not found in third_party/librga${NC}"
        echo -e "${YELLOW}Consider installing rockchip RGA for hardware acceleration${NC}"
    fi
}

# Function to install NVIDIA GPU dependencies (x86_64)
install_nvidia_gpu_deps() {
    echo -e "${YELLOW}Installing NVIDIA GPU dependencies...${NC}"
    echo -e "${YELLOW}Note: CUDA and TensorRT should be installed manually from NVIDIA.${NC}"
    echo -e "${YELLOW}Visit: https://developer.nvidia.com/cuda-downloads${NC}"
}

# Function to install Jetson dependencies
install_jetson_deps() {
    echo -e "${YELLOW}Installing Jetson-specific dependencies...${NC}"
    echo -e "${YELLOW}Note: CUDA and TensorRT are typically pre-installed with JetPack.${NC}"
    
    # Install additional jetson utilities if available
    if command -v apt-cache &> /dev/null; then
        if apt-cache show nvidia-jetpack &> /dev/null 2>&1; then
            echo -e "${GREEN}JetPack detected on this system${NC}"
        fi
    fi
}

# Function to install optional dependencies
install_optional_deps() {
    echo -e "${YELLOW}Installing optional dependencies...${NC}"
    
    read -p "Install Kafka support (librdkafka)? [y/N]: " install_kafka
    if [[ "$install_kafka" =~ ^[Yy]$ ]]; then
        sudo apt-get install -y librdkafka-dev
        echo -e "${GREEN}Kafka support installed${NC}"
    fi
    
    read -p "Install MQTT support (libmosquitto)? [y/N]: " install_mqtt
    if [[ "$install_mqtt" =~ ^[Yy]$ ]]; then
        sudo apt-get install -y libmosquitto-dev
        echo -e "${GREEN}MQTT support installed${NC}"
    fi
    
    read -p "Install FFmpeg development libraries? [y/N]: " install_ffmpeg
    if [[ "$install_ffmpeg" =~ ^[Yy]$ ]]; then
        sudo apt-get install -y \
            libavcodec-dev \
            libavformat-dev \
            libavdevice-dev \
            libswscale-dev \
            libswresample-dev \
            libavutil-dev
        echo -e "${GREEN}FFmpeg support installed${NC}"
    fi
}

# Function to print build instructions
print_build_instructions() {
    echo ""
    echo -e "${BLUE}==========================================${NC}"
    echo -e "${BLUE} Build Instructions ${NC}"
    echo -e "${BLUE}==========================================${NC}"
    
    case $PLATFORM in
        rockchip)
            echo -e "${GREEN}For Rockchip RK35xx:${NC}"
            echo "  mkdir build && cd build"
            echo "  cmake -DCVEDIX_WITH_RKNN=ON -DCVEDIX_WITH_RGA=ON .."
            echo "  make -j\$(nproc)"
            ;;
        jetson)
            echo -e "${GREEN}For NVIDIA Jetson:${NC}"
            echo "  mkdir build && cd build"
            echo "  cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_TRT=ON .."
            echo "  make -j\$(nproc)"
            ;;
        nvidia_gpu)
            echo -e "${GREEN}For NVIDIA GPU (x86_64):${NC}"
            echo "  mkdir build && cd build"
            echo "  cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_TRT=ON .."
            echo "  make -j\$(nproc)"
            ;;
        cpu_only)
            echo -e "${GREEN}For CPU only:${NC}"
            echo "  mkdir build && cd build"
            echo "  cmake .."
            echo "  make -j\$(nproc)"
            ;;
    esac
    
    echo ""
    echo -e "${YELLOW}Available CMake Options:${NC}"
    echo "  -DCVEDIX_WITH_GSTREAMER=ON/OFF  # GStreamer support (default: ON)"
    echo "  -DCVEDIX_WITH_CUDA=ON/OFF       # CUDA support"
    echo "  -DCVEDIX_WITH_TRT=ON/OFF        # TensorRT support"
    echo "  -DCVEDIX_WITH_RKNN=ON/OFF       # Rockchip NPU support"
    echo "  -DCVEDIX_WITH_RGA=ON/OFF        # Rockchip RGA support"
    echo "  -DCVEDIX_WITH_KAFKA=ON/OFF      # Kafka support"
    echo "  -DCVEDIX_WITH_MQTT=ON/OFF       # MQTT support"
    echo "  -DCVEDIX_WITH_LLM=ON/OFF        # LLM support"
    echo "  -DCVEDIX_WITH_FFMPEG=ON/OFF     # FFmpeg support"
    echo "  -DCVEDIX_BUILD_SAMPLES=ON/OFF   # Build samples"
}

# Main installation flow
main() {
    # Check if running as root or with sudo capability
    if ! sudo -v &> /dev/null; then
        echo -e "${RED}Error: This script requires sudo privileges${NC}"
        exit 1
    fi
    
    # Install base dependencies
    install_base_deps
    
    # Install GStreamer
    install_gstreamer
    
    # Install platform-specific dependencies
    case $PLATFORM in
        rockchip)
            install_rockchip_deps
            ;;
        jetson)
            install_jetson_deps
            ;;
        nvidia_gpu)
            install_nvidia_gpu_deps
            ;;
        cpu_only)
            echo -e "${GREEN}CPU-only build - no additional hardware dependencies needed${NC}"
            ;;
    esac
    
    # Ask about optional dependencies
    echo ""
    read -p "Install optional dependencies (Kafka, MQTT, FFmpeg)? [y/N]: " install_optional
    if [[ "$install_optional" =~ ^[Yy]$ ]]; then
        install_optional_deps
    fi
    
    # Print build instructions
    print_build_instructions
    
    echo ""
    echo -e "${GREEN}==========================================${NC}"
    echo -e "${GREEN} Setup Complete! ${NC}"
    echo -e "${GREEN}==========================================${NC}"
}

# Run main function
main
