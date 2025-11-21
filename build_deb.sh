#!/bin/bash
set -e

####################################
# Script để build .deb package cho CVEDIX SDK
# Usage: ./build_deb.sh [--version=VERSION] [--arch=ARCH]
####################################

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
PACKAGE_VERSION="1.0.0"
DEB_VERSION="1"
ARCH=$(dpkg --print-architecture)

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            PACKAGE_VERSION="$2"
            shift 2
            ;;
        --arch)
            ARCH="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --version=VERSION    Package version (default: 1.0.0)"
            echo "  --arch=ARCH          Architecture (default: auto-detect)"
            echo "  --help              Show this help message"
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
echo -e "${GREEN}  CVEDIX INSTANCE SDK Debian Package Builder   ${NC}"
echo -e "${GREEN}===============================================${NC}"
echo ""
echo -e "Package version: ${YELLOW}$PACKAGE_VERSION${NC}"
echo -e "Debian revision: ${YELLOW}$DEB_VERSION${NC}"
echo -e "Architecture: ${YELLOW}$ARCH${NC}"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "debian" ]; then
    echo -e "${RED}Error: Must run from project root directory${NC}"
    exit 1
fi

# Check for required tools
echo -e "${GREEN}[1/5] Checking required tools...${NC}"
# Check dpkg-buildpackage
if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    echo -e "${RED}Error: dpkg-buildpackage is not installed${NC}"
    echo "Install with: sudo apt-get install dpkg-dev"
    exit 1
fi
# Check debhelper (provides 'dh' command, not 'debhelper')
if ! command -v dh >/dev/null 2>&1; then
    echo -e "${RED}Error: debhelper is not installed (dh command not found)${NC}"
    echo "Install with: sudo apt-get install debhelper"
    exit 1
fi
# Check cmake
if ! command -v cmake >/dev/null 2>&1; then
    echo -e "${RED}Error: cmake is not installed${NC}"
    echo "Install with: sudo apt-get install cmake"
    exit 1
fi

# Update changelog with version
echo -e "${GREEN}[2/5] Updating changelog...${NC}"
if [ -f "debian/changelog" ]; then
    # Create a temporary changelog with new version
    cat > debian/changelog.new << EOF
cvedix-instance-sdk (${PACKAGE_VERSION}-${DEB_VERSION}) unstable; urgency=medium

  * Release version ${PACKAGE_VERSION}
  * Built for architecture ${ARCH}

 -- CVEDIX Team <team@cvedix.com>  $(date -R)

EOF
    # Append old changelog entries
    cat debian/changelog >> debian/changelog.new
    mv debian/changelog.new debian/changelog
fi

# Clean previous builds
echo -e "${GREEN}[3/5] Cleaning previous builds...${NC}"
rm -rf debian/cvedix-instance-sdk*
rm -rf debian/.debhelper
rm -rf debian/files
rm -rf debian/*.substvars
rm -rf ../cvedix-instance-sdk_*

# Build the package
echo -e "${GREEN}[4/5] Building Debian package...${NC}"
echo "This may take a while..."
dpkg-buildpackage -b -us -uc

if [ $? -ne 0 ]; then
    echo -e "${RED}Package build failed!${NC}"
    exit 1
fi

# Find the generated .deb files
echo -e "${GREEN}[5/5] Package build completed!${NC}"
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Build Summary${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# List generated files
DEB_FILES=$(find .. -maxdepth 1 -name "cvedix-instance-sdk*.deb" -type f)
if [ -n "$DEB_FILES" ]; then
    echo -e "Generated package files:"
    for file in $DEB_FILES; do
        SIZE=$(du -h "$file" | cut -f1)
        echo -e "  ${YELLOW}$(basename $file)${NC} (${SIZE})"
    done
    echo ""
    echo -e "Package location: ${YELLOW}$(dirname $(realpath $(echo $DEB_FILES | cut -d' ' -f1)))${NC}"
    echo ""
    echo -e "To install the package:"
    echo -e "  ${YELLOW}sudo dpkg -i ../cvedix-instance-sdk_${PACKAGE_VERSION}-${DEB_VERSION}_${ARCH}.deb${NC}"
    echo -e "  ${YELLOW}sudo dpkg -i ../cvedix-instance-sdk-dev_${PACKAGE_VERSION}-${DEB_VERSION}_${ARCH}.deb${NC}"
    echo ""
    echo -e "Or install both at once:"
    echo -e "  ${YELLOW}sudo apt-get install ../cvedix-instance-sdk*.deb${NC}"
else
    echo -e "${RED}Warning: No .deb files found!${NC}"
fi

echo ""

