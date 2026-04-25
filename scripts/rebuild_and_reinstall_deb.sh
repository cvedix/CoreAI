#!/bin/bash
set -e

#########################################
# Rebuild and Reinstall Debian Package
# This script rebuilds the package with latest code and reinstalls it
#########################################

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  Rebuild and Reinstall Debian Package      ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

# Configuration
BUILD_DIR="build_pkg_cpu"
BUILD_SAMPLES=${BUILD_SAMPLES:-ON}
BUILD_TYPE=${BUILD_TYPE:-Release}
KEEP_MODELS=false
MODEL_OPTION="2"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --keep-models)
            KEEP_MODELS=true
            shift
            ;;
        --model-option=*)
            MODEL_OPTION="${1#*=}"
            shift
            ;;
        --build-samples=*)
            BUILD_SAMPLES="${1#*=}"
            shift
            ;;
        --build-type=*)
            BUILD_TYPE="${1#*=}"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --keep-models        Keep existing cvedix_data (don't regenerate)"
            echo "  --model-option=N     Model selection (1=all, 2=ONNX only, 3=minimal)"
            echo "  --build-samples=ON|OFF  Build samples (default: ON)"
            echo "  --build-type=TYPE    Build type: Debug or Release (default: Release)"
            echo "  --help, -h           Show this help"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Check if package is installed
INSTALLED_PKG=$(dpkg -l | grep "^ii.*cvedix-ai-runtime " | awk '{print $2}' || true)
if [ -n "$INSTALLED_PKG" ]; then
    INSTALLED_VERSION=$(dpkg -l | grep "^ii.*cvedix-ai-runtime " | awk '{print $3}')
    echo -e "${BLUE}Found installed package: ${YELLOW}${INSTALLED_PKG}${NC} (version: ${INSTALLED_VERSION})"
    echo ""
    
    read -p "Uninstall existing package? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${BLUE}Uninstalling existing package...${NC}"
        sudo dpkg -r $INSTALLED_PKG 2>/dev/null || sudo apt-get remove -y $INSTALLED_PKG
        echo -e "${GREEN}✓ Package uninstalled${NC}"
    else
        echo -e "${YELLOW}⚠ Warning: Keeping old package installed${NC}"
        echo -e "${YELLOW}New package will be installed alongside (may cause conflicts)${NC}"
    fi
    echo ""
fi

# Check source directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: Not in project root directory${NC}"
    exit 1
fi

# Prepare cvedix_data if needed
if [ "$KEEP_MODELS" = false ]; then
    echo -e "${BLUE}[1/6] Checking cvedix_data...${NC}"
    if [ ! -d "cvedix_data" ]; then
        echo -e "${YELLOW}cvedix_data not found. Preparing models...${NC}"
        if [ -f "build_cpu_deb_with_models.sh" ]; then
            # Use existing script but skip package build
            CVEDIX_DATA_SOURCE=${CVEDIX_DATA_SOURCE:-"./build/bin/cvedix_data"}
            if [ -d "${CVEDIX_DATA_SOURCE}" ]; then
                echo -e "${BLUE}Creating cvedix_data with ONNX models...${NC}"
                mkdir -p cvedix_data/models cvedix_data/video
                
                # Copy ONNX models
                find "${CVEDIX_DATA_SOURCE}/models" -name "*.onnx" -type f 2>/dev/null | while read file; do
                    rel_path="${file#${CVEDIX_DATA_SOURCE}/models/}"
                    target_dir="cvedix_data/models/$(dirname "$rel_path")"
                    mkdir -p "$target_dir"
                    cp "$file" "$target_dir/"
                done
                
                # Copy labels and config files
                find "${CVEDIX_DATA_SOURCE}/models" \( -name "*.txt" -o -name "*.json" \) -type f 2>/dev/null | while read file; do
                    rel_path="${file#${CVEDIX_DATA_SOURCE}/models/}"
                    target_dir="cvedix_data/models/$(dirname "$rel_path")"
                    mkdir -p "$target_dir"
                    cp "$file" "$target_dir/"
                done
                
                echo -e "${GREEN}✓ cvedix_data prepared${NC}"
            fi
        fi
    else
        echo -e "${GREEN}✓ cvedix_data exists${NC}"
    fi
    echo ""
fi

# Clean build directory
if [ -d "$BUILD_DIR" ]; then
    echo -e "${BLUE}[2/6] Cleaning previous build...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

# Configure CMake
echo -e "${BLUE}[3/6] Configuring CMake...${NC}"
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_TRT=OFF \
    -DCVEDIX_WITH_RKNN=OFF \
    -DCVEDIX_WITH_RGA=OFF \
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
echo -e "${BLUE}[4/6] Building SDK...${NC}"
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build completed${NC}"

# Create package
echo ""
echo -e "${BLUE}[5/6] Creating Debian package...${NC}"
cpack -G DEB

if [ $? -ne 0 ]; then
    echo -e "${RED}Package creation failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Package created${NC}"

# Find package file
DEB_FILE=$(ls cvedix-ai-runtime*.deb 2>/dev/null | head -1)
if [ -z "$DEB_FILE" ]; then
    echo -e "${RED}Error: Package file not found!${NC}"
    exit 1
fi

# Verify package has updated paths
echo ""
echo -e "${BLUE}[6/6] Verifying package contents...${NC}"
PKG_INCLUDE_PATH=$(dpkg-deb -c "$DEB_FILE" 2>/dev/null | grep "cvedix_objects_cereal_archive.h" | awk '{print $6}' | head -1)
if [ -n "$PKG_INCLUDE_PATH" ]; then
    # Extract and check
    TEMP_EXTRACT=$(mktemp -d)
    dpkg-deb -x "$DEB_FILE" "$TEMP_EXTRACT" 2>/dev/null
    CHECK_FILE="$TEMP_EXTRACT$PKG_INCLUDE_PATH"
    
    if [ -f "$CHECK_FILE" ]; then
        if grep -q "cvedix/third_party/cereal" "$CHECK_FILE" 2>/dev/null; then
            echo -e "${GREEN}✓ Package has updated include paths${NC}"
        elif grep -q "\.\./.*third_party/cereal" "$CHECK_FILE" 2>/dev/null; then
            echo -e "${RED}✗ Package still has old relative paths!${NC}"
            echo -e "${YELLOW}  This shouldn't happen - source code should be updated${NC}"
        fi
    fi
    rm -rf "$TEMP_EXTRACT"
fi

# Install package
echo ""
read -p "Install new package? (Y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    echo -e "${BLUE}Installing package...${NC}"
    
    # Check if same version is already installed
    NEW_VERSION=$(dpkg -I "$DEB_FILE" 2>/dev/null | grep "^[[:space:]]*Version:" | awk '{print $2}')
    INSTALLED_VERSION=$(dpkg -l cvedix-ai-runtime 2>/dev/null | grep "^ii" | awk '{print $3}' || echo "")
    
    if [ -n "$INSTALLED_VERSION" ] && [ "$INSTALLED_VERSION" = "$NEW_VERSION" ]; then
        echo -e "${YELLOW}⚠ Same version ($NEW_VERSION) already installed${NC}"
        echo -e "${BLUE}Forcing reinstall to replace old files...${NC}"
        sudo apt-get install --reinstall -y "./$DEB_FILE"
    else
        # Different version - normal install/upgrade
        if [ -n "$INSTALLED_VERSION" ]; then
            echo -e "${BLUE}Upgrading from version $INSTALLED_VERSION to $NEW_VERSION...${NC}"
        fi
        sudo apt-get install -y "./$DEB_FILE"
    fi
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Package installed successfully${NC}"
        
        # Verify installation
        echo ""
        echo -e "${BLUE}Verifying installation...${NC}"
        INSTALLED_FILE="/opt/cvedix/include/cvedix/nodes/broker/cereal_archive/cvedix_objects_cereal_archive.h"
        if [ -f "$INSTALLED_FILE" ]; then
            if grep -q "cvedix/third_party/cereal" "$INSTALLED_FILE" 2>/dev/null; then
                echo -e "${GREEN}✓ Installed files have updated paths${NC}"
            else
                echo -e "${YELLOW}⚠ Warning: Installed file still has old paths${NC}"
                echo -e "${YELLOW}  File location: $INSTALLED_FILE${NC}"
            fi
        fi
    else
        echo -e "${RED}Package installation failed!${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Skipping installation${NC}"
fi

# Show results
echo ""
echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  Rebuild and Reinstall Completed!          ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

SIZE=$(du -h "$DEB_FILE" | cut -f1)
echo -e "Package file: ${YELLOW}${DEB_FILE}${NC}"
echo -e "Size:         ${YELLOW}${SIZE}${NC}"
echo -e "Location:     ${YELLOW}$(pwd)/${DEB_FILE}${NC}"
echo ""

echo -e "${BLUE}Package info:${NC}"
dpkg -I "$DEB_FILE" | grep -E "Package|Version|Architecture" | sed 's/^/ /'
echo ""

echo -e "${BLUE}To install manually:${NC}"
echo -e "  ${YELLOW}sudo apt-get install ./${DEB_FILE}${NC}"
echo ""

echo -e "${BLUE}To verify installed paths:${NC}"
echo -e "  ${YELLOW}grep -r 'cvedix/third_party' /opt/cvedix/include/cvedix/nodes/broker/cereal_archive/${NC}"
echo ""

echo -e "${GREEN}✅ All done!${NC}"
echo ""

