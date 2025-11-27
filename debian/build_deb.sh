#!/bin/bash
set -e

####################################
# Script để build .deb package sử dụng debuild/dpkg-buildpackage
# Usage: ./debian/build_deb.sh [OPTIONS]
# 
# Options:
#   --clean              Clean build directory before building
#   --release            Build in Release mode (default)
#   --debug              Build in Debug mode
#   --help               Show this help message
####################################

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=false
BUILD_DIR="../build_package"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --clean               Clean build directory before building"
            echo "  --release             Build in Release mode (default)"
            echo "  --debug               Build in Debug mode"
            echo "  --help, -h            Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Build with default options"
            echo "  $0 --clean --release                 # Clean build in Release mode"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${GREEN}===============================================${NC}"
echo -e "${GREEN}  CVEDIX AI Runtime Debian Package Builder   ${NC}"
echo -e "${GREEN}===============================================${NC}"
echo ""
echo -e "Build type:      ${YELLOW}${BUILD_TYPE}${NC}"

# Check if we're in the debian directory
if [ ! -f "control" ]; then
    echo -e "${RED}Error: Must run from debian directory${NC}"
    echo "Usage: cd debian && ./build_deb.sh"
    exit 1
fi

# Go to project root
cd ..

# Check for required tools
echo -e "${BLUE}[1/5] Checking required tools...${NC}"
REQUIRED_TOOLS=("dpkg-buildpackage" "debhelper")
MISSING_TOOLS=()

for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [ ${#MISSING_TOOLS[@]} -ne 0 ]; then
    echo -e "${RED}Error: Missing required tools: ${MISSING_TOOLS[*]}${NC}"
    echo "Install with: sudo apt-get install build-essential debhelper dpkg-dev"
    exit 1
fi
echo -e "${GREEN}✓ All required tools are available${NC}"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${BLUE}[2/5] Cleaning build directory...${NC}"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo -e "${GREEN}✓ Build directory cleaned${NC}"
    fi
fi

# Export build type for debian/rules
export BUILD_TYPE

# Build package
echo -e "${BLUE}[3/5] Building Debian package...${NC}"
echo "This may take a while..."

# Use dpkg-buildpackage to build the package
dpkg-buildpackage -b -us -uc

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

DEB_FILES=$(find .. -maxdepth 1 -name "*.deb" -type f)
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
    echo -e "To install the packages:"
    echo -e "  ${YELLOW}cd ${ABS_DIR} && sudo dpkg -i *.deb${NC}"
    echo ""
    echo -e "Or install with apt:"
    echo -e "  ${YELLOW}cd ${ABS_DIR} && sudo apt-get install ./cvedix-ai-runtime*.deb${NC}"
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
echo -e "${GREEN}✅ All done!${NC}"

