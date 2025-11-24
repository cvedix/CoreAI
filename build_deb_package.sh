#!/bin/bash
set -e

####################################
# Script để build .deb package cho CVEDIX SDK sử dụng CPack
# Usage: ./build_deb_package.sh [OPTIONS]
# 
# Options:
#   --version=VERSION    Package version (default: 2025.0.1.2)
#   --build-type=TYPE    Build type: Debug or Release (default: Release)
#   --clean              Clean build directory before building
#   --help               Show this help message
####################################

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
PACKAGE_VERSION="2025.0.1.2"
BUILD_TYPE="Release"
BUILD_DIR="build_package"
CLEAN_BUILD=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --version=*)
            PACKAGE_VERSION="${1#*=}"
            shift
            ;;
        --version)
            PACKAGE_VERSION="$2"
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
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --version=VERSION     Package version (default: 2025.0.1.2)"
            echo "  --build-type=TYPE     Build type: Debug or Release (default: Release)"
            echo "  --clean               Clean build directory before building"
            echo "  --help, -h            Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Build with default options"
            echo "  $0 --version=2025.0.1.2              # Build with specific version"
            echo "  $0 --build-type=Release --clean       # Clean build in Release mode"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Validate build type
if [[ "$BUILD_TYPE" != "Debug" && "$BUILD_TYPE" != "Release" ]]; then
    echo -e "${RED}Error: Build type must be 'Debug' or 'Release'${NC}"
    exit 1
fi

echo -e "${GREEN}===============================================${NC}"
echo -e "${GREEN}  CVEDIX AI Runtime Debian Package Builder   ${NC}"
echo -e "${GREEN}===============================================${NC}"
echo ""
echo -e "Package version: ${YELLOW}${PACKAGE_VERSION}${NC}"
echo -e "Build type:      ${YELLOW}${BUILD_TYPE}${NC}"
echo -e "Build directory: ${YELLOW}${BUILD_DIR}${NC}"

# Detect architecture
if [ -z "$ARCH" ]; then
    ARCH=$(uname -m)
    if [ "$ARCH" = "aarch64" ]; then
        ARCH="arm64"
    elif [ "$ARCH" = "x86_64" ]; then
        ARCH="x86_64"
    fi
fi
echo -e "Architecture:     ${YELLOW}${ARCH}${NC}"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: Must run from project root directory${NC}"
    exit 1
fi

# Check for required tools
echo -e "${BLUE}[1/6] Checking required tools...${NC}"
REQUIRED_TOOLS=("cmake" "make" "cpack")
MISSING_TOOLS=()

for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [ ${#MISSING_TOOLS[@]} -ne 0 ]; then
    echo -e "${RED}Error: Missing required tools: ${MISSING_TOOLS[*]}${NC}"
    echo "Install with: sudo apt-get install cmake build-essential"
    exit 1
fi
echo -e "${GREEN}✓ All required tools are available${NC}"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${BLUE}[2/6] Cleaning build directory...${NC}"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo -e "${GREEN}✓ Build directory cleaned${NC}"
    else
        echo -e "${YELLOW}⚠ Build directory does not exist, skipping clean${NC}"
    fi
else
    echo -e "${BLUE}[2/6] Checking build directory...${NC}"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake with package version
echo -e "${BLUE}[3/6] Configuring CMake...${NC}"
cmake \
    -D CMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -D PROJECT_VERSION="$PACKAGE_VERSION" \
    ..

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ CMake configuration completed${NC}"

# Build the project
echo -e "${BLUE}[4/6] Building project...${NC}"
echo "This may take a while..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build completed${NC}"

# Install to staging area
echo -e "${BLUE}[5/6] Installing to staging area...${NC}"
make install DESTDIR=./install_staging

if [ $? -ne 0 ]; then
    echo -e "${RED}Installation failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Installation completed${NC}"

# Build Debian package with CPack
echo -e "${BLUE}[6/6] Building Debian package with CPack...${NC}"
cpack -G DEB

if [ $? -ne 0 ]; then
    echo -e "${RED}Package build failed!${NC}"
    exit 1
fi

# Find the generated .deb files
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Build Summary${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

DEB_FILES=$(find . -maxdepth 1 -name "*.deb" -type f)
if [ -n "$DEB_FILES" ]; then
    echo -e "${GREEN}✓ Package build completed successfully!${NC}"
    echo ""
    echo -e "Generated package files:"
    for file in $DEB_FILES; do
        SIZE=$(du -h "$file" | cut -f1)
        FILENAME=$(basename "$file")
        echo -e "  ${YELLOW}${FILENAME}${NC} (${SIZE})"
    done
    echo ""
    
    # Get absolute path
    ABS_PATH=$(realpath "$(echo $DEB_FILES | cut -d' ' -f1)")
    ABS_DIR=$(dirname "$ABS_PATH")
    
    echo -e "Package location: ${YELLOW}${ABS_DIR}${NC}"
    echo ""
    
    # Detect architecture from package name
    ARCH_FROM_PKG=$(echo "$FIRST_DEB" | grep -oE "(arm64|x86_64|amd64|i386)" | head -1)
    if [ -z "$ARCH_FROM_PKG" ]; then
        # Try to get from dpkg info
        ARCH_FROM_PKG=$(dpkg -I "$FIRST_DEB" 2>/dev/null | grep "Architecture:" | awk '{print $2}' || echo "unknown")
    fi
    
    echo -e "Architecture: ${YELLOW}${ARCH_FROM_PKG}${NC}"
    echo ""
    echo -e "To install the package:"
    echo -e "  ${YELLOW}sudo dpkg -i ${ABS_DIR}/*.deb${NC}"
    echo ""
    echo -e "Or install with apt:"
    if [ "$ARCH_FROM_PKG" != "unknown" ]; then
        echo -e "  ${YELLOW}cd ${ABS_DIR} && sudo apt-get install ./cvedix-ai-runtime-${PACKAGE_VERSION}-${ARCH_FROM_PKG}.deb${NC}"
    else
        echo -e "  ${YELLOW}cd ${ABS_DIR} && sudo apt-get install ./cvedix-ai-runtime-${PACKAGE_VERSION}-*.deb${NC}"
    fi
    echo ""
    echo -e "To check package contents:"
    echo -e "  ${YELLOW}dpkg -c ${ABS_DIR}/*.deb${NC}"
    echo -e "  ${YELLOW}dpkg -I ${ABS_DIR}/*.deb${NC}"
else
    echo -e "${RED}Warning: No .deb files found!${NC}"
    echo "Check the build output above for errors."
    exit 1
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Package Information${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Show package info for first .deb file
FIRST_DEB=$(echo $DEB_FILES | cut -d' ' -f1)
if [ -n "$FIRST_DEB" ]; then
    echo -e "${BLUE}Package details:${NC}"
    dpkg -I "$FIRST_DEB" | grep -E "Package|Version|Architecture|Depends|Description" | head -10
    echo ""
    
    # Check package contents
    echo -e "${BLUE}Package contents summary:${NC}"
    echo -e "  ${YELLOW}Libraries:${NC} /opt/cvedix/lib/"
    echo -e "  ${YELLOW}Headers:${NC} /opt/cvedix/include/cvedix/"
    echo -e "  ${YELLOW}Samples:${NC} /opt/cvedix/bin/"
    
    # Check if cvedix_data is included
    if dpkg -c "$FIRST_DEB" | grep -q "cvedix_data"; then
        echo -e "  ${GREEN}✓ cvedix_data:${NC} /opt/cvedix/bin/cvedix_data/"
    else
        echo -e "  ${YELLOW}⚠ cvedix_data:${NC} Not included (directory not found in source)"
    fi
    
    # Check if release notes are included
    if dpkg -c "$FIRST_DEB" | grep -q "RELEASE_NOTES"; then
        echo -e "  ${GREEN}✓ Release Notes:${NC} /opt/cvedix/share/doc/cvedix-ai-runtime/RELEASE_NOTES.md"
    fi
    
    echo -e "  ${YELLOW}CMake Config:${NC} /opt/cvedix/lib/cmake/cvedix/"
fi

echo ""
echo -e "${GREEN}✅ All done!${NC}"

