# Hướng dẫn Đóng gói Package cho Rockchip ARM64

## Tổng quan

SDK hỗ trợ đóng gói thành Debian package (.deb) cho Rockchip ARM64 hardware với đầy đủ tính năng:
- GStreamer với MPP hardware codecs
- RKNN Runtime (Rockchip NPU)
- RGA (Rockchip Graphics Accelerator)
- FFmpeg (optional)
- LLM support (optional)

## Yêu cầu

### 1. Build dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    debhelper \
    dpkg-dev
```

### 2. SDK dependencies

```bash
sudo apt-get install -y \
    libopencv-dev \
    libopencv-contrib-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    libgstrtspserver-1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-rtsp \
    gstreamer1.0-tools
```

### 3. Rockchip-specific (optional)

```bash
# GStreamer MPP plugins cho Rockchip
sudo apt-get install -y gstreamer1.0-rockchip1

# RKNN runtime (nếu có trong third_party/rknn/ thì không cần)
# Hoặc cài đặt thủ công từ rknn-toolkit2

# RGA library (nếu có trong third_party/librga/ thì không cần)
```

## Phương pháp 1: Sử dụng CPack (Khuyến nghị)

### Bước 1: Configure CMake với Rockchip options

```bash
cd /home/ubuntu/core_ai_runtime
mkdir build && cd build

# Full build cho Rockchip với tất cả features
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_WITH_RGA=ON \
    -DCVEDIX_WITH_FFMPEG=ON \
    -DCVEDIX_BUILD_SAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    ..
```

### Bước 2: Build

```bash
make -j$(nproc)
```

### Bước 3: Tạo package

```bash
make package
```

Hoặc với CPack trực tiếp:

```bash
cpack -G DEB
```

### Kết quả:

```
build/cvedix-ai-runtime-2025.0.1.2-arm64.deb
```

## Phương pháp 2: Sử dụng dpkg-buildpackage

### Bước 1: Chạy build script

```bash
cd /home/ubuntu/core_ai_runtime/debian
./build_deb.sh --clean --release
```

### Bước 2: Kiểm tra output

```bash
ls -lh ../*.deb
```

### Kết quả:

```
../cvedix-ai-runtime_2025.0.1.2_arm64.deb
../cvedix-ai-runtime-dev_2025.0.1.2_arm64.deb
```

## Thông tin Package

### Package name và version

- **Name**: `cvedix-ai-runtime`
- **Version**: `2025.0.1.2` (từ CMakeLists.txt project version)
- **Architecture**: `arm64` (tự động detect từ `CMAKE_SYSTEM_PROCESSOR`)
- **File name**: `cvedix-ai-runtime-2025.0.1.2-arm64.deb`

### Package content

**Runtime package (`cvedix-ai-runtime`):**
```
/opt/cvedix/
├── lib/
│   ├── libcvedix_instance_sdk.so
│   ├── rknn/librknnrt.so (if RKNN enabled)
│   └── rga/librga.so (if RGA enabled)
├── bin/
│   ├── *_sample (if samples built)
│   └── cvedix_data/ (models, test data)
└── share/
    └── doc/
```

**Dev package (`cvedix-ai-runtime-dev`):**
```
/opt/cvedix/
├── include/
│   └── cvedix/
│       ├── nodes/
│       ├── objects/
│       ├── utils/
│       └── excepts/
├── lib/
│   └── cmake/cvedix/
│       ├── cvedix-targets.cmake
│       └── cvedix-config.cmake
└── lib/
    └── pkgconfig/
        └── cvedix.pc
```

## Cấu hình Package cho Rockchip

### Enable RKNN support

CMake sẽ tự động detect RKNN từ `third_party/rknn/`:

```cmake
# Trong CMakeLists.txt (dòng 864-866)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
    # RKNN automatically bundled if available
endif()
```

### Dependencies của package

Package tự động detect dependencies (với `CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON`):

```
Depends: 
  gstreamer1.0-plugins-base (>= 1.14.5),
  gstreamer1.0-plugins-good (>= 1.14.5),
  gstreamer1.0-plugins-bad (>= 1.14.5),
  libgstreamer1.0-0 (>= 1.14.5),
  libgstreamer-plugins-base1.0-0 (>= 1.14.5),
  libopencv-core4.5 (auto-detected),
  ...

Recommends:
  gstreamer1.0-rtsp,
  gstreamer1.0-plugins-ugly
```

## Build với các options khác nhau

### 1. Minimal package (chỉ GStreamer):

```bash
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_RKNN=OFF \
    -DCVEDIX_WITH_RGA=OFF \
    -DCVEDIX_BUILD_SAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    ..

make -j$(nproc)
make package
```

**File**: `cvedix-ai-runtime-2025.0.1.2-arm64.deb` (~2-3 MB)

### 2. Full Rockchip package (GStreamer + RKNN + RGA):

```bash
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_WITH_RGA=ON \
    -DCVEDIX_BUILD_SAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    ..

make -j$(nproc)
make package
```

**File**: `cvedix-ai-runtime-2025.0.1.2-arm64.deb` (~5-10 MB)

### 3. Full package với samples:

```bash
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_WITH_RGA=ON \
    -DCVEDIX_WITH_FFMPEG=ON \
    -DCVEDIX_WITH_LLM=ON \
    -DCVEDIX_BUILD_SAMPLES=ON \
    -DCMAKE_BUILD_TYPE=Release \
    ..

make -j$(nproc)
make package
```

**File**: `cvedix-ai-runtime-2025.0.1.2-arm64.deb` (~50-100 MB with samples and cvedix_data)

## Cài đặt Package trên Rockchip

### Bước 1: Copy package đến target device

```bash
# Từ build machine
scp build/cvedix-ai-runtime-2025.0.1.2-arm64.deb ubuntu@rockchip-device:/tmp/
```

### Bước 2: Cài đặt trên Rockchip device

```bash
# SSH vào Rockchip device
ssh ubuntu@rockchip-device

# Cài đặt package
cd /tmp
sudo apt-get install ./cvedix-ai-runtime-2025.0.1.2-arm64.deb

# Hoặc với dpkg
sudo dpkg -i cvedix-ai-runtime-2025.0.1.2-arm64.deb
sudo apt-get install -f  # Fix dependencies
```

### Bước 3: Verify cài đặt

```bash
# Kiểm tra library
ls -lh /opt/cvedix/lib/libcvedix_instance_sdk.so

# Kiểm tra RKNN (nếu enabled)
ls -lh /opt/cvedix/lib/rknn/librknnrt.so

# Kiểm tra samples (nếu có)
ls -lh /opt/cvedix/bin/*_sample

# Test một sample
cd /opt/cvedix/bin
./rknn_yolov11_detector_sample
```

## Tạo Repository Package

### Để phân phối qua apt repository:

```bash
# 1. Tạo directory structure
mkdir -p my_repo/pool/main
cp *.deb my_repo/pool/main/

# 2. Tạo Packages index
cd my_repo
dpkg-scanpackages pool/main /dev/null | gzip -9c > pool/main/Packages.gz

# 3. Tạo Release file
cat > Release << EOF
Origin: CVEDIX
Label: CVEDIX AI Runtime
Suite: stable
Codename: rockchip
Version: 2025.0.1.2
Architectures: arm64
Components: main
Description: CVEDIX AI Runtime for Rockchip
EOF

# 4. Host repository (nginx, apache, hoặc simple HTTP server)
python3 -m http.server 8000
```

### Sử dụng repository:

```bash
# Trên target devices
echo "deb [trusted=yes] http://your-server:8000/my_repo pool/main/" | \
    sudo tee /etc/apt/sources.list.d/cvedix.list

sudo apt-get update
sudo apt-get install cvedix-ai-runtime
```

## Build Script mẫu

Tạo file `build_rockchip_package.sh`:

```bash
#!/bin/bash
set -e

echo "======================================"
echo "Building CVEDIX Package for Rockchip"
echo "======================================"

# Clean previous build
rm -rf build_pkg
mkdir build_pkg && cd build_pkg

# Configure for Rockchip ARM64
cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_RKNN=ON \
    -DCVEDIX_WITH_RGA=ON \
    -DCVEDIX_WITH_FFMPEG=OFF \
    -DCVEDIX_BUILD_SAMPLES=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    ..

# Build
echo "Building..."
make -j$(nproc)

# Create package
echo "Creating package..."
make package

# Show result
echo ""
echo "======================================"
echo "Package created successfully!"
echo "======================================"
ls -lh cvedix-ai-runtime-*.deb
echo ""
echo "Install with:"
echo "  sudo apt-get install ./cvedix-ai-runtime-*.deb"
echo "======================================"
```

Chạy:
```bash
chmod +x build_rockchip_package.sh
./build_rockchip_package.sh
```

## Kiểm tra Package

### 1. Xem metadata

```bash
dpkg -I cvedix-ai-runtime-2025.0.1.2-arm64.deb
```

Output:
```
Package: cvedix-ai-runtime
Version: 2025.0.1.2
Section: devel
Priority: optional
Architecture: arm64
Depends: gstreamer1.0-plugins-base, ...
Installed-Size: 15000
Maintainer: CVEDIX Team <support@cvedix.com>
Description: CVEDIX AI Runtime SDK with RKNN, MQTT and JSON Output Support
```

### 2. Xem nội dung

```bash
dpkg -c cvedix-ai-runtime-2025.0.1.2-arm64.deb | head -20
```

### 3. Extract để kiểm tra

```bash
dpkg -x cvedix-ai-runtime-2025.0.1.2-arm64.deb /tmp/check_pkg
tree /tmp/check_pkg/opt/cvedix
```

## Troubleshooting

### Lỗi: "dpkg-shlibdeps: error"

Dependencies không tìm thấy. Giải pháp:

```bash
# Tắt auto dependency detection
cmake .. -DCPACK_DEBIAN_PACKAGE_SHLIBDEPS=OFF
```

Hoặc cài đặt missing dependencies trước khi build.

### Lỗi: "Architecture mismatch"

Đảm bảo build trên đúng platform (Rockchip ARM64):

```bash
uname -m
# Phải ra: aarch64

# Kiểm tra CMake detect
cmake .. 2>&1 | grep "ARCHITECTURE"
# Phải thấy: arm64
```

### Package quá lớn

Nếu package quá lớn (do samples và cvedix_data):

```bash
# Build không có samples
cmake .. -DCVEDIX_BUILD_SAMPLES=OFF

# Hoặc không đóng gói cvedix_data
# Xóa hoặc di chuyển cvedix_data ra ngoài source tree
```

## Phân phối Package

### Option 1: Direct install

Copy .deb file và cài đặt trực tiếp:

```bash
scp *.deb user@target-device:/tmp/
ssh user@target-device
cd /tmp
sudo apt-get install ./cvedix-ai-runtime*.deb
```

### Option 2: HTTP repository

Host package trên HTTP server:

```bash
# Tạo simple repository
mkdir -p repo
cp *.deb repo/
cd repo
python3 -m http.server 8000

# Trên client devices
wget http://your-ip:8000/cvedix-ai-runtime-2025.0.1.2-arm64.deb
sudo apt-get install ./cvedix-ai-runtime*.deb
```

### Option 3: APT repository

Setup proper APT repository (xem section "Tạo Repository Package" ở trên).

## Post-install Verification

Sau khi cài đặt package:

```bash
# 1. Kiểm tra library
ls -lh /opt/cvedix/lib/libcvedix_instance_sdk.so

# 2. Kiểm tra RKNN
ls -lh /opt/cvedix/lib/rknn/librknnrt.so

# 3. Kiểm tra RGA
ls -lh /opt/cvedix/lib/rga/librga.so

# 4. Update library cache
sudo ldconfig

# 5. Test library
ldd /opt/cvedix/lib/libcvedix_instance_sdk.so | grep -E "(rknn|rga|gstreamer)"

# 6. Test sample (nếu có)
cd /opt/cvedix/bin
./rknn_yolov11_detector_sample --help
```

## Quick Reference

### Build commands

| Command | Description |
|---------|-------------|
| `cmake .. && make && make package` | Build và đóng gói |
| `cpack -G DEB` | Tạo DEB package |
| `dpkg -I *.deb` | Xem thông tin package |
| `dpkg -c *.deb` | Xem nội dung package |
| `sudo apt-get install ./*.deb` | Cài đặt package |
| `sudo apt-get remove cvedix-ai-runtime` | Gỡ cài đặt |

### Package locations

| Component | Location |
|-----------|----------|
| **Library** | `/opt/cvedix/lib/libcvedix_instance_sdk.so` |
| **Headers** | `/opt/cvedix/include/cvedix/` |
| **Binaries** | `/opt/cvedix/bin/` |
| **Data** | `/opt/cvedix/bin/cvedix_data/` |
| **CMake config** | `/opt/cvedix/lib/cmake/cvedix/` |
| **pkg-config** | `/opt/cvedix/lib/pkgconfig/cvedix.pc` |

### Environment setup (nếu cần)

```bash
# Add to ~/.bashrc
export PATH="/opt/cvedix/bin:$PATH"
export LD_LIBRARY_PATH="/opt/cvedix/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="/opt/cvedix/lib/pkgconfig:$PKG_CONFIG_PATH"
```

## Tài liệu tham khảo

- [CPack Documentation](https://cmake.org/cmake/help/latest/module/CPack.html)
- [Debian Package Guide](https://www.debian.org/doc/manuals/maint-guide/)
- [dpkg-buildpackage](https://manpages.debian.org/testing/dpkg-dev/dpkg-buildpackage.1.en.html)

---

**Last Updated**: 2025-12-07  
**Target**: Rockchip ARM64 (RK3566/RK3568/RK3588)
**Package Format**: Debian (.deb)


