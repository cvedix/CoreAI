# Đóng gói Package với RKNN Models

## Tổng quan

Hướng dẫn đóng gói SDK kèm theo `cvedix_data` chứa RKNN models và test data.

**Lưu ý**: `cvedix_data` có thể rất lớn (~3GB với tất cả models). Có 3 options:
1. Đóng gói tất cả (package lớn ~3-4GB)
2. Đóng gói chỉ RKNN models cần thiết (package ~100-500MB)
3. Tạo package riêng cho models

## Option 1: Đóng gói tất cả models (Full Package)

### Bước 1: Chuẩn bị cvedix_data

```bash
cd /home/ubuntu/core_ai_runtime

# Copy cvedix_data từ build directory
cp -r ./build/bin/cvedix_data ./cvedix_data

# Hoặc tạo symlink (nhanh hơn)
ln -s ./build/bin/cvedix_data ./cvedix_data
```

### Bước 2: Kiểm tra nội dung

```bash
# Kiểm tra kích thước
du -sh ./cvedix_data

# Liệt kê RKNN models
find ./cvedix_data/models/rknn -name "*.rknn" -type f
```

### Bước 3: Build package

```bash
./build_rockchip_package.sh
```

**Kết quả:**
```
cvedix-ai-runtime-2025.0.1.2-arm64.deb (~3-4GB)
```

**Cài đặt vào**: `/opt/cvedix/bin/cvedix_data/`

## Option 2: Đóng gói chỉ RKNN models (Recommended)

Tạo cvedix_data minimal chỉ với RKNN models:

### Bước 1: Tạo cvedix_data minimal

```bash
cd /home/ubuntu/core_ai_runtime

# Tạo structure
mkdir -p cvedix_data/models/rknn/rk3588
mkdir -p cvedix_data/models/face
mkdir -p cvedix_data/test_video

# Copy chỉ RKNN models cần thiết
cp -r ./build/bin/cvedix_data/models/rknn/rk3588/*.rknn \
    ./cvedix_data/models/rknn/rk3588/ 2>/dev/null || true

# Copy face models
cp -r ./build/bin/cvedix_data/models/face/*.rknn \
    ./cvedix_data/models/face/ 2>/dev/null || true

# Copy labels files (nhỏ)
cp ./build/bin/cvedix_data/models/coco_80_labels_list.txt \
    ./cvedix_data/models/ 2>/dev/null || true

# Copy 1-2 test videos nhỏ (tùy chọn)
cp ./build/bin/cvedix_data/test_video/face.mp4 \
    ./cvedix_data/test_video/ 2>/dev/null || true
```

### Bước 2: Kiểm tra kích thước

```bash
du -sh ./cvedix_data
# Kỳ vọng: ~100-500MB (chỉ RKNN models)
```

### Bước 3: Build package

```bash
./build_rockchip_package.sh
```

**Kết quả:**
```
cvedix-ai-runtime-2025.0.1.2-arm64.deb (~100-500MB)
```

## Option 3: Package riêng cho models

Tạo 2 packages:
1. SDK package (không có models) - nhỏ
2. Models package (chỉ models) - lớn

### Package 1: SDK (không có models)

```bash
cd /home/ubuntu/core_ai_runtime

# Đảm bảo không có cvedix_data trong source
rm -f cvedix_data  # Xóa symlink nếu có

# Build
./build_rockchip_package.sh
```

**Output**: `cvedix-ai-runtime-2025.0.1.2-arm64.deb` (~5-10MB, không có models)

### Package 2: Models package (thủ công)

```bash
# Tạo models package structure
mkdir -p models_pkg/opt/cvedix/bin/cvedix_data

# Copy models
cp -r ./build/bin/cvedix_data/models \
    models_pkg/opt/cvedix/bin/cvedix_data/

# Tạo control file
mkdir -p models_pkg/DEBIAN
cat > models_pkg/DEBIAN/control << 'EOF'
Package: cvedix-ai-runtime-models
Version: 2025.0.1.2
Section: devel
Priority: optional
Architecture: arm64
Depends: cvedix-ai-runtime (= 2025.0.1.2)
Maintainer: CVEDIX Team <support@cvedix.com>
Description: RKNN Models for CVEDIX AI Runtime
 Pre-trained RKNN models for Rockchip NPU including:
 - YOLOv8/v11 object detection models
 - Face detection models
 - Other AI models
EOF

# Build package
dpkg-deb --build models_pkg cvedix-ai-runtime-models-2025.0.1.2-arm64.deb
```

**Output**: `cvedix-ai-runtime-models-2025.0.1.2-arm64.deb` (~1-3GB)

**Cài đặt cả 2:**
```bash
sudo apt-get install \
    ./cvedix-ai-runtime-2025.0.1.2-arm64.deb \
    ./cvedix-ai-runtime-models-2025.0.1.2-arm64.deb
```

## Script build với models

Tạo file `build_rockchip_package_with_models.sh`:

```bash
#!/bin/bash
set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}=============================================${NC}"
echo -e "${GREEN}  Build Rockchip Package WITH Models        ${NC}"
echo -e "${GREEN}=============================================${NC}"

# Check if cvedix_data exists
if [ ! -d "cvedix_data" ]; then
    echo -e "${BLUE}cvedix_data not found, preparing...${NC}"
    
    if [ -d "./build/bin/cvedix_data" ]; then
        echo -e "${YELLOW}Option 1: Copy all data (3GB)${NC}"
        echo -e "${YELLOW}Option 2: Copy only RKNN models (~500MB)${NC}"
        echo -e "${YELLOW}Option 3: Create symlink${NC}"
        echo ""
        read -p "Choose option (1/2/3): " choice
        
        case $choice in
            1)
                echo -e "${BLUE}Copying all cvedix_data...${NC}"
                cp -r ./build/bin/cvedix_data ./cvedix_data
                echo -e "${GREEN}✓ Copied (~3GB)${NC}"
                ;;
            2)
                echo -e "${BLUE}Creating minimal cvedix_data...${NC}"
                mkdir -p cvedix_data/models/rknn/rk3588
                mkdir -p cvedix_data/models/face
                mkdir -p cvedix_data/test_video
                
                cp -r ./build/bin/cvedix_data/models/rknn/rk3588/*.rknn \
                    ./cvedix_data/models/rknn/rk3588/ 2>/dev/null || true
                cp -r ./build/bin/cvedix_data/models/face/*.rknn \
                    ./cvedix_data/models/face/ 2>/dev/null || true
                cp ./build/bin/cvedix_data/models/*.txt \
                    ./cvedix_data/models/ 2>/dev/null || true
                    
                echo -e "${GREEN}✓ Created minimal (~500MB)${NC}"
                ;;
            3)
                echo -e "${BLUE}Creating symlink...${NC}"
                ln -s ./build/bin/cvedix_data ./cvedix_data
                echo -e "${GREEN}✓ Symlink created${NC}"
                ;;
            *)
                echo -e "${RED}Invalid choice${NC}"
                exit 1
                ;;
        esac
    else
        echo -e "${RED}Error: cvedix_data not found in ./build/bin/${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✓ cvedix_data already exists${NC}"
fi

# Show cvedix_data size
echo ""
SIZE=$(du -sh cvedix_data | cut -f1)
echo -e "cvedix_data size: ${YELLOW}${SIZE}${NC}"
echo ""

# Run build script
./build_rockchip_package.sh

echo ""
echo -e "${GREEN}Package with models created!${NC}"
```

Chạy:
```bash
chmod +x build_rockchip_package_with_models.sh
./build_rockchip_package_with_models.sh
```

## Liệt kê RKNN models trong package

Sau khi tạo package, kiểm tra models có trong package:

```bash
# Xem tất cả files .rknn trong package
dpkg -c cvedix-ai-runtime-2025.0.1.2-arm64.deb | grep "\.rknn$"

# Kết quả mong đợi:
# /opt/cvedix/bin/cvedix_data/models/rknn/rk3588/yolov11s.rknn
# /opt/cvedix/bin/cvedix_data/models/face/yolov8n_face_detection.rknn
# ...
```

## Cài đặt và sử dụng

### Cài đặt package

```bash
sudo apt-get install ./cvedix-ai-runtime-2025.0.1.2-arm64.deb
```

### Verify models sau khi cài đặt

```bash
# Liệt kê RKNN models
ls -lh /opt/cvedix/bin/cvedix_data/models/rknn/rk3588/*.rknn

# Kiểm tra labels
ls -lh /opt/cvedix/bin/cvedix_data/models/*.txt

# Test với sample
cd /opt/cvedix/bin
./rknn_yolov11_detector_sample
```

## Kích thước Package

| Variant | Size | Includes |
|---------|------|----------|
| **SDK only** | ~5-10 MB | Libraries, headers, binaries |
| **SDK + RKNN models** | ~100-500 MB | SDK + RKNN models only |
| **SDK + Full cvedix_data** | ~3-4 GB | SDK + all models, test videos, data |

## Khuyến nghị

✅ **Cho development**: Dùng Option 1 (full package) - có tất cả để test  
✅ **Cho production**: Dùng Option 2 (chỉ RKNN models cần thiết) - nhẹ hơn  
✅ **Cho deployment**: Dùng Option 3 (2 packages riêng) - linh hoạt  

---

**Last Updated**: 2025-12-07  
**Target**: Rockchip ARM64

