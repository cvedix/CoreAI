# Build Debian Package cho CPU (x86_64/amd64)

## Tổng quan

Hướng dẫn đóng gói CVEDIX AI Runtime SDK cho CPU-only builds (không sử dụng TensorRT, RKNN, hay FFmpeg), kèm theo ONNX models.

## Scripts có sẵn

### 1. `build_cpu_deb_package.sh`
Script chính để build Debian package.

**Tính năng:**
- Build SDK với cấu hình CPU-only
- Tự động phát hiện cvedix_data nếu có
- Tạo .deb package với CPack
- Hỗ trợ samples

**Sử dụng:**
```bash
# Build package cơ bản (không có models)
./build_cpu_deb_package.sh

# Build với cấu hình tùy chỉnh
BUILD_SAMPLES=OFF ./build_cpu_deb_package.sh
BUILD_TYPE=Debug ./build_cpu_deb_package.sh
```

### 2. `build_cpu_package_with_models.sh`
Script tự động chuẩn bị cvedix_data và gọi script build.

**Tính năng:**
- Hỗ trợ 3 options để chuẩn bị models:
  - Option 1: Copy tất cả data (~3GB)
  - Option 2: Copy tất cả ONNX models (~800MB) - Khuyến nghị
  - Option 3: Copy minimal ONNX models (~200MB) - Face detection only
- Tự động đếm và hiển thị ONNX models
- Gọi build_cpu_deb_package.sh để tạo package

**Sử dụng:**
```bash
# Chạy script (sẽ hỏi lựa chọn)
./build_cpu_package_with_models.sh

# Hoặc chọn trước với input redirect
echo "2" | ./build_cpu_package_with_models.sh  # Option 2
echo "3" | ./build_cpu_package_with_models.sh  # Option 3
```

## Yêu cầu hệ thống

### Build Dependencies
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake (>= 3.10) \
    pkg-config \
    libopencv-dev (>= 4.6) \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstrtspserver-1.0-dev
```

## Quy trình build đầy đủ

### Bước 1: Build project
```bash
mkdir build && cd build

cmake \
    -DCVEDIX_WITH_GSTREAMER=ON \
    -DCVEDIX_WITH_TRT=OFF \
    -DCVEDIX_WITH_RKNN=OFF \
    -DCVEDIX_BUILD_SAMPLES=ON \
    ..

make -j$(nproc)
cd ..
```

### Bước 2: Chuẩn bị models và build package
```bash
# Tự động với script
./build_cpu_package_with_models.sh

# Hoặc thủ công
# 2a. Chuẩn bị cvedix_data
mkdir -p cvedix_data/models/face
cp build/bin/cvedix_data/models/face/*.onnx cvedix_data/models/face/

# 2b. Build package
./build_cpu_deb_package.sh
```

## Cấu trúc Package

Package được tạo tại: `build_pkg_cpu/cvedix-ai-runtime-*.deb`

**Nội dung:**
```
/opt/cvedix/
├── lib/
│   ├── libcvedix_instance_sdk.so    # Main library (~40MB)
│   └── libtinyexpr.so               # Tiny library
├── include/cvedix/                  # Headers
│   ├── nodes/
│   ├── objects/
│   └── utils/
├── bin/
│   ├── *_sample                     # 30+ samples
│   └── cvedix_data/                 # Models (nếu có)
│       └── models/
│           ├── face/*.onnx          # Face models
│           ├── det_cls/*.onnx       # Detection/Classification
│           └── *.txt                # Labels
├── lib/cmake/cvedix/                # CMake config
└── lib/pkgconfig/                   # pkg-config
```

## Cài đặt Package

### Cài đặt
```bash
cd build_pkg_cpu

# Khuyến nghị: Dùng apt-get (tự động xử lý dependencies)
sudo apt-get install ./cvedix-ai-runtime-*.deb

# Hoặc dùng dpkg
sudo dpkg -i cvedix-ai-runtime-*.deb
sudo apt-get install -f  # Fix dependencies nếu cần
```

### Kiểm tra
```bash
# Xem package info
dpkg -l | grep cvedix

# Xem files đã cài
dpkg -L cvedix-ai-runtime

# Xem ONNX models
find /opt/cvedix -name "*.onnx"

# List samples
ls /opt/cvedix/bin/*_sample
```

### Chạy samples
```bash
cd /opt/cvedix/bin

# Face detection
./face_tracking_sample

# YOLOv11 ONNX detector
./yolov11_onnx_detector_sample

# InsightFace recognition
./insightface_sample
```

## Gỡ cài đặt

```bash
# Gỡ package
sudo apt-get remove cvedix-ai-runtime

# Gỡ hoàn toàn (bao gồm config)
sudo apt-get purge cvedix-ai-runtime

# Xóa dependencies không dùng
sudo apt-get autoremove
```

## Sử dụng trong project

### CMake
```cmake
find_package(cvedix REQUIRED)
target_link_libraries(your_app PRIVATE cvedix::cvedix_instance_sdk)
```

### pkg-config
```bash
g++ main.cpp $(pkg-config --cflags --libs cvedix) -o my_app
```

### Trực tiếp
```bash
g++ main.cpp \
    -I/opt/cvedix/include \
    -L/opt/cvedix/lib \
    -lcvedix_instance_sdk \
    -o my_app
```

## Advanced Options

### Tùy chỉnh build
```bash
# Build Release (default)
BUILD_TYPE=Release ./build_cpu_deb_package.sh

# Build Debug
BUILD_TYPE=Debug ./build_cpu_deb_package.sh

# Build without samples (nhanh hơn)
BUILD_SAMPLES=OFF ./build_cpu_deb_package.sh

# Tùy chỉnh nguồn cvedix_data
CVEDIX_DATA_SOURCE="/custom/path" ./build_cpu_deb_package.sh
```

### Kiểm tra package trước khi cài

```bash
cd build_pkg_cpu

# Xem thông tin tổng quát
dpkg -I cvedix-ai-runtime-*.deb

# Xem danh sách files
dpkg -c cvedix-ai-runtime-*.deb

# Xem dependencies
dpkg -I cvedix-ai-runtime-*.deb | grep Depends

# Đếm ONNX models
dpkg -c cvedix-ai-runtime-*.deb | grep -c "\.onnx"

# Xem kích thước
du -h cvedix-ai-runtime-*.deb
```

## Troubleshooting

### Lỗi: CMake configuration failed
```bash
# Kiểm tra dependencies
sudo apt-get install -f
sudo apt-get install --reinstall libopencv-dev
```

### Lỗi: No ONNX models in package
```bash
# Xóa và tạo lại cvedix_data
rm -rf cvedix_data
./build_cpu_package_with_models.sh
```

### Lỗi: Package too large
```bash
# Chỉ đóng gói models cần thiết
rm -rf cvedix_data
mkdir -p cvedix_data/models/face
cp build/bin/cvedix_data/models/face/*.onnx cvedix_data/models/face/
./build_cpu_deb_package.sh
```

### Lỗi build
```bash
# Clean và rebuild
rm -rf build build_pkg_cpu cvedix_data
mkdir build && cd build
cmake -DCVEDIX_WITH_GSTREAMER=ON \
      -DCVEDIX_BUILD_SAMPLES=ON ..
make -j$(nproc)
cd ..
./build_cpu_package_with_models.sh
```

## So sánh CPU vs Rockchip Package

| Feature | CPU Package | Rockchip Package |
|---------|-------------|------------------|
| **Script** | `build_cpu_deb_package.sh` | `build_rockchip_package.sh` |
| **Architecture** | x86_64/amd64 | ARM64 (aarch64) |
| **Model Format** | ONNX | RKNN + ONNX |
| **Backend** | OpenCV DNN | RKNN Runtime |
| **Hardware** | Any CPU | Rockchip NPU |
| **Speed** | Moderate | Very Fast |
| **Portability** | High | Rockchip only |
| **Dependencies** | OpenCV, GStreamer | OpenCV, GStreamer, RKNN |

## Tips

1. **Giảm kích thước package**: Chỉ copy models cần thiết (option 3)
2. **Build nhanh**: Tắt samples với `BUILD_SAMPLES=OFF`
3. **Debug**: Build với `BUILD_TYPE=Debug` để debug dễ hơn
4. **Phát triển**: Không cần package, dùng trực tiếp từ `build/`
5. **Production**: Build Release với tất cả models cần thiết

## File Structure

```
core_ai_runtime/
├── build_cpu_deb_package.sh           # Script chính build package
├── build_cpu_package_with_models.sh   # Script chuẩn bị models + build
├── README_CPU_PACKAGE.md              # File này
├── build/                             # Build directory
│   └── bin/
│       └── cvedix_data/              # Source models
├── cvedix_data/                       # Models để đóng gói (tạo bởi script)
└── build_pkg_cpu/                     # Output directory
    └── *.deb                          # Package file
```

## Support

- Documentation: `doc/` folder
- Examples: `samples/` folder
- Issues: Check build logs in `build_pkg_cpu/`

