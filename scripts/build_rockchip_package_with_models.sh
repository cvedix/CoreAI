#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  Build Rockchip Package WITH Models        ${NC}"
echo -e "${GREEN}=============================================${NC}"
echo ""

# Check if cvedix_data exists
if [ ! -d "cvedix_data" ]; then
    echo -e "${BLUE}cvedix_data not found in source directory${NC}"
    echo ""
    
    if [ -d "./build/bin/cvedix_data" ]; then
        echo -e "${YELLOW}Available options:${NC}"
        echo -e "  ${BLUE}1)${NC} Copy all data (~3GB) - Includes all models, videos, test data"
        echo -e "  ${BLUE}2)${NC} Copy only RKNN models (~500MB) - Recommended for packaging"
        echo -e "  ${BLUE}3)${NC} Create symlink - Fast but symlink may break in package"
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
                echo -e "${BLUE}Creating minimal cvedix_data with RKNN models...${NC}"
                mkdir -p cvedix_data/models/rknn/rk3588
                mkdir -p cvedix_data/models/face
                mkdir -p cvedix_data/models/det_cls
                mkdir -p cvedix_data/test_video
                
                # Copy RKNN models
                echo "Copying RKNN models..."
                cp -r ./build/bin/cvedix_data/models/rknn/rk3588/*.rknn \
                    ./cvedix_data/models/rknn/rk3588/ 2>/dev/null || true
                    
                cp ./build/bin/cvedix_data/models/face/*.rknn \
                    ./cvedix_data/models/face/ 2>/dev/null || true
                
                # Copy labels files (small)
                echo "Copying labels..."
                cp ./build/bin/cvedix_data/models/*.txt \
                    ./cvedix_data/models/ 2>/dev/null || true
                    
                cp ./build/bin/cvedix_data/models/det_cls/*.txt \
                    ./cvedix_data/models/det_cls/ 2>/dev/null || true
                
                # Copy one small test video (optional)
                echo "Copying test video..."
                cp ./build/bin/cvedix_data/test_video/face.mp4 \
                    ./cvedix_data/test_video/ 2>/dev/null || true
                    
                echo -e "${GREEN}✓ Created minimal cvedix_data${NC}"
                ;;
            3)
                echo -e "${BLUE}Creating symlink...${NC}"
                ln -sf ./build/bin/cvedix_data ./cvedix_data
                echo -e "${GREEN}✓ Symlink created${NC}"
                echo -e "${YELLOW}⚠ Warning: Symlinks may not work correctly in packages${NC}"
                ;;
            *)
                echo -e "${RED}Invalid choice${NC}"
                exit 1
                ;;
        esac
    else
        echo -e "${RED}Error: cvedix_data not found in ./build/bin/${NC}"
        echo -e "${YELLOW}Please ensure you have built the project first:${NC}"
        echo -e "  cd build && make"
        exit 1
    fi
else
    echo -e "${GREEN}✓ cvedix_data already exists in source directory${NC}"
fi

# Show cvedix_data size
echo ""
SIZE=$(du -sh cvedix_data 2>/dev/null | cut -f1)
echo -e "cvedix_data size: ${YELLOW}${SIZE}${NC}"

# Count RKNN models
RKNN_COUNT=$(find cvedix_data -name "*.rknn" 2>/dev/null | wc -l)
echo -e "RKNN models found: ${YELLOW}${RKNN_COUNT}${NC}"
echo ""

# Run build script
echo -e "${BLUE}Starting package build...${NC}"
echo ""
./build_rockchip_package.sh

echo ""
echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}Package with models created successfully!${NC}"
echo -e "${GREEN}=============================================${NC}"


