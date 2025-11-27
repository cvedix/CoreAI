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
BUILD_SAMPLES=false
PARALLEL_JOBS=$(nproc)

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
        --samples)
            BUILD_SAMPLES=true
            shift
            ;;
        --jobs|-j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --clean               Clean build directory before building"
            echo "  --release             Build in Release mode (default)"
            echo "  --debug               Build in Debug mode"
            echo "  --samples             Build samples (disabled by default to save time/resources)"
            echo "  --jobs, -j N          Limit parallel build jobs (default: all CPU cores)"
            echo "  --help, -h            Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Build with default options (no samples)"
            echo "  $0 --clean --release                 # Clean build in Release mode"
            echo "  $0 --samples                         # Build with samples included"
            echo "  $0 --jobs 2                          # Build with max 2 parallel jobs"
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
echo -e "Build samples:   ${YELLOW}$([ "$BUILD_SAMPLES" = true ] && echo "Yes" || echo "No (saves time/resources)")${NC}"
echo -e "Parallel jobs:   ${YELLOW}${PARALLEL_JOBS}${NC}"

# Check execution directory and ensure we are in project root
if [ -f "debian/control" ]; then
    # We are in the project root
    echo -e "${BLUE}Running from project root...${NC}"
elif [ -f "control" ]; then
    # We are in the debian directory
    echo -e "${BLUE}Running from debian directory, moving to root...${NC}"
    cd ..
else
    echo -e "${RED}Error: Cannot determine project root.${NC}"
    echo "Please run from the project root or the debian directory."
    exit 1
fi

# Check for required tools
echo -e "${BLUE}[1/5] Checking required tools...${NC}"
MISSING_TOOLS=()

# Check for dpkg-buildpackage
if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    MISSING_TOOLS+=("dpkg-buildpackage (from dpkg-dev)")
fi

# Check for dh (from debhelper)
if ! command -v dh >/dev/null 2>&1; then
    MISSING_TOOLS+=("dh (from debhelper)")
fi

# Check for cmake
if ! command -v cmake >/dev/null 2>&1; then
    MISSING_TOOLS+=("cmake")
fi

# Check for make
if ! command -v make >/dev/null 2>&1; then
    MISSING_TOOLS+=("make (from build-essential)")
fi

if [ ${#MISSING_TOOLS[@]} -ne 0 ]; then
    echo -e "${RED}Error: Missing required tools:${NC}"
    for tool in "${MISSING_TOOLS[@]}"; do
        echo -e "  ${RED}- ${tool}${NC}"
    done
    echo ""
    echo -e "${YELLOW}To install all required packages, run:${NC}"
    echo -e "  ${GREEN}sudo apt-get update${NC}"
    echo -e "  ${GREEN}sudo apt-get install -y build-essential debhelper dpkg-dev cmake${NC}"
    echo ""
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

# Export build configuration for debian/rules
export BUILD_TYPE
export BUILD_SAMPLES
export PARALLEL_JOBS

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


