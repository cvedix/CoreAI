#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  Build CPU Package WITH ONNX Models        ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

# Check if cvedix_data exists
if [ ! -d "cvedix_data" ]; then
    echo -e "${BLUE}cvedix_data not found in source directory${NC}"
    echo ""
    
    if [ -d "./build/bin/cvedix_data" ]; then
        echo -e "${YELLOW}Available options:${NC}"
        echo -e "  ${BLUE}1)${NC} Copy all data (~3GB) - Includes all models, videos, test data"
        echo -e "  ${BLUE}2)${NC} Copy only ONNX models (~800MB) - Recommended for CPU packaging"
        echo -e "  ${BLUE}3)${NC} Copy minimal ONNX models (~200MB) - Face detection + YOLOv11 only"
        echo ""
        read -p "Choose option (1/2/3): " choice
        
        case $choice in
            1)
                echo -e "${BLUE}Copying all cvedix_data (~3GB)...${NC}"
                echo "This may take a few minutes..."
                cp -r ./build/bin/cvedix_data ./cvedix_data
                echo -e "${GREEN}✓ Copied all data${NC}"
                ;;
            2)
                echo -e "${BLUE}Creating cvedix_data with all ONNX models...${NC}"
                mkdir -p cvedix_data/models
                mkdir -p cvedix_data/test_video
                
                # Copy all ONNX models recursively
                echo "Copying ONNX models..."
                find ./build/bin/cvedix_data/models -name "*.onnx" -type f | while read file; do
                    rel_path="${file#./build/bin/cvedix_data/models/}"
                    target_dir="cvedix_data/models/$(dirname "$rel_path")"
                    mkdir -p "$target_dir"
                    cp "$file" "$target_dir/"
                done
                
                # Copy labels files (small)
                echo "Copying labels..."
                find ./build/bin/cvedix_data/models -name "*.txt" -type f | while read file; do
                    rel_path="${file#./build/bin/cvedix_data/models/}"
                    target_dir="cvedix_data/models/$(dirname "$rel_path")"
                    mkdir -p "$target_dir"
                    cp "$file" "$target_dir/"
                done
                
                # Copy test videos (optional)
                echo "Copying test videos..."
                if [ -d "./build/bin/cvedix_data/test_video" ]; then
                    cp -r ./build/bin/cvedix_data/test_video/*.mp4 \
                        ./cvedix_data/test_video/ 2>/dev/null || true
                fi
                    
                echo -e "${GREEN}✓ Created cvedix_data with ONNX models${NC}"
                ;;
            3)
                echo -e "${BLUE}Creating minimal cvedix_data with essential ONNX models...${NC}"
                mkdir -p cvedix_data/models/face/face_recognition
                mkdir -p cvedix_data/test_video
                
                # Copy essential face detection models
                echo "Copying face detection models..."
                cp ./build/bin/cvedix_data/models/face/face_detection_yunet_2023mar.onnx \
                   ./cvedix_data/models/face/ 2>/dev/null || true
                cp ./build/bin/cvedix_data/models/face/face_detection_yolov11.onnx \
                   ./cvedix_data/models/face/ 2>/dev/null || true
                
                # Copy face recognition models
                echo "Copying face recognition models..."
                cp ./build/bin/cvedix_data/models/face/face_recognition/*.onnx \
                   ./cvedix_data/models/face/face_recognition/ 2>/dev/null || true
                
                # Copy labels
                echo "Copying labels..."
                cp ./build/bin/cvedix_data/models/coco_80classes.txt \
                   ./cvedix_data/models/ 2>/dev/null || true
                
                # Copy one test video
                echo "Copying test video..."
                cp ./build/bin/cvedix_data/test_video/face.mp4 \
                   ./cvedix_data/test_video/ 2>/dev/null || true
                    
                echo -e "${GREEN}✓ Created minimal cvedix_data${NC}"
                ;;
            *)
                echo -e "${RED}Invalid choice${NC}"
                exit 1
                ;;
        esac
    else
        echo -e "${RED}Error: cvedix_data not found in ./build/bin/${NC}"
        echo -e "${YELLOW}Please ensure you have built the project first:${NC}"
        echo -e "  mkdir build && cd build"
        echo -e "  cmake -DCVEDIX_WITH_GSTREAMER=ON \\"
        echo -e "        -DCVEDIX_BUILD_SAMPLES=ON .."
        echo -e "  make -j\$(nproc)"
        exit 1
    fi
else
    echo -e "${GREEN}✓ cvedix_data already exists in source directory${NC}"
fi

# Show cvedix_data size
echo ""
SIZE=$(du -sh cvedix_data 2>/dev/null | cut -f1)
echo -e "cvedix_data size: ${YELLOW}${SIZE}${NC}"

# Count ONNX models
ONNX_COUNT=$(find cvedix_data -name "*.onnx" 2>/dev/null | wc -l)
echo -e "ONNX models found: ${YELLOW}${ONNX_COUNT}${NC}"
echo ""

# Run Debian package build script
echo -e "${BLUE}Starting Debian package build...${NC}"
echo ""

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -f "$PROJECT_ROOT/scripts/build_cpu_deb_package.sh" ]; then
    cd "$PROJECT_ROOT"
    chmod +x scripts/build_cpu_deb_package.sh
    ./scripts/build_cpu_deb_package.sh
elif [ -f "$SCRIPT_DIR/../scripts/build_cpu_deb_package.sh" ]; then
    cd "$SCRIPT_DIR/.."
    chmod +x scripts/build_cpu_deb_package.sh
    ./scripts/build_cpu_deb_package.sh
else
    echo -e "${RED}Error: build_cpu_deb_package.sh not found in scripts/ directory${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}Debian package with ONNX models created!${NC}"
echo -e "${GREEN}=============================================${NC}"
