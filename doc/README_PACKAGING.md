# Hướng dẫn đóng gói Debian Package

## Tổng quan

CVEDIX AI Runtime SDK hỗ trợ đóng gói thành Debian package (.deb) sử dụng CPack. Package được cấu hình để cài đặt vào `/opt/cvedix` để tránh xung đột với hệ thống.

## Yêu cầu

- CMake >= 3.10
- Build tools (make, g++, etc.)
- CPack (thường đi kèm với CMake)

## Cách build package

### Phương pháp 1: Sử dụng script (Khuyến nghị)

```bash
# Build với version mặc định (2025.0.1.2)
./build_deb_package.sh

# Build với version cụ thể
./build_deb_package.sh --version=2025.0.1.2

# Build với build type Release và clean build directory
./build_deb_package.sh --build-type=Release --clean

# Xem help
./build_deb_package.sh --help
```

### Phương pháp 2: Build thủ công

```bash
# 1. Tạo build directory
mkdir -p build_package
cd build_package

# 2. Configure CMake
cmake -D CMAKE_BUILD_TYPE=Release \
      -D PROJECT_VERSION=2025.0.1.2 \
      ..

# 3. Build project
make -j$(nproc)

# 4. Install to staging
make install DESTDIR=./install_staging

# 5. Build Debian package
cpack -G DEB
```

## Cấu trúc package

Package sẽ chứa:

- **Libraries**: `/opt/cvedix/lib/libcvedix_instance_sdk.so`
- **Headers**: `/opt/cvedix/include/cvedix/`
- **Samples**: `/opt/cvedix/bin/` (tất cả samples được build)
- **Data**: `/opt/cvedix/bin/cvedix_data/` (nếu có trong source directory)
- **Release Notes**: `/opt/cvedix/share/doc/cvedix-ai-runtime/RELEASE_NOTES.md`
- **CMake config**: `/opt/cvedix/lib/cmake/cvedix/`
- **pkg-config**: `/opt/cvedix/lib/pkgconfig/cvedix.pc`

## Cài đặt package

```bash
# Cài đặt package (thay thế arm64 hoặc x86_64 bằng architecture của bạn)
sudo dpkg -i cvedix-ai-runtime-2025.0.1.2-arm64.deb
# hoặc
sudo dpkg -i cvedix-ai-runtime-2025.0.1.2-x86_64.deb

# Hoặc sử dụng apt
sudo apt-get install ./cvedix-ai-runtime-2025.0.1.2-arm64.deb
# hoặc
sudo apt-get install ./cvedix-ai-runtime-2025.0.1.2-x86_64.deb

# Fix dependencies nếu cần
sudo apt-get install -f
```

## Kiểm tra package

```bash
# Xem thông tin package (thay thế arm64 hoặc x86_64)
dpkg -I cvedix-ai-runtime-2025.0.1.2-arm64.deb

# Xem nội dung package
dpkg -c cvedix-ai-runtime-2025.0.1.2-arm64.deb

# Kiểm tra package đã cài đặt
dpkg -l | grep cvedix

# Kiểm tra architecture của package
dpkg -I cvedix-ai-runtime-2025.0.1.2-*.deb | grep Architecture
```

## Gỡ cài đặt

```bash
sudo apt-get remove cvedix-ai-runtime
# hoặc
sudo dpkg -r cvedix-ai-runtime
```

## Version Information

Version được định nghĩa trong `CMakeLists.txt`:

```cmake
project(cvedix_instance_sdk VERSION 2025.0.1.2)
```

CPack sẽ tự động sử dụng `PROJECT_VERSION` cho package version.

## Package File Naming

Package file sẽ có tên theo format:
- **arm64**: `cvedix-ai-runtime-2025.0.1.2-arm64.deb`
- **x86_64**: `cvedix-ai-runtime-2025.0.1.2-x86_64.deb`
- **amd64**: `cvedix-ai-runtime-2025.0.1.2-amd64.deb` (Debian convention cho x86_64)

Architecture được tự động detect từ hệ thống build.

## Dependencies

Package tự động detect dependencies thông qua `CPACK_DEBIAN_PACKAGE_SHLIBDEPS`. Các dependencies bắt buộc:

- gstreamer1.0-plugins-base (>= 1.14.5)
- gstreamer1.0-plugins-good (>= 1.14.5)
- gstreamer1.0-plugins-bad (>= 1.14.5)
- libgstreamer1.0-0 (>= 1.14.5)
- libgstreamer-plugins-base1.0-0 (>= 1.14.5)

Dependencies khuyến nghị:

- gstreamer1.0-rtsp
- gstreamer1.0-plugins-ugly

## Architecture Support

Package tự động detect architecture:

- `aarch64` → `arm64`
- `x86_64` → `amd64`
- `i386/i686` → `i386`

## Troubleshooting

### Lỗi: "CPack Error: Could not find CPack"

Đảm bảo CMake version >= 3.10 và CPack được enable.

### Lỗi: "Missing dependencies"

Cài đặt các dependencies bắt buộc:

```bash
sudo apt-get update
sudo apt-get install gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
                     gstreamer1.0-plugins-bad libgstreamer1.0-0 \
                     libgstreamer-plugins-base1.0-0
```

### Lỗi: "Package version mismatch"

Kiểm tra version trong `CMakeLists.txt` và đảm bảo `PROJECT_VERSION` được set đúng.

## Build với các tùy chọn

Để build package với các features tùy chọn (RKNN, MQTT, etc.):

```bash
cd build_package
cmake -D CMAKE_BUILD_TYPE=Release \
      -D PROJECT_VERSION=2025.0.1.2 \
      -D CVEDIX_WITH_RKNN=ON \
      -D CVEDIX_WITH_MQTT=ON \
      ..
make -j$(nproc)
cpack -G DEB
```

## Notes

- Package được cài đặt vào `/opt/cvedix` để tránh xung đột với hệ thống
- Để sử dụng SDK sau khi cài đặt, cần set `CMAKE_PREFIX_PATH=/opt/cvedix` hoặc sử dụng `find_package(cvedix)`
- Package name: `cvedix-ai-runtime`
- Package version format: `2025.0.1.2`

## Package Contents

### Included Components

1. **Libraries & Headers**
   - Main SDK library và tất cả header files
   - CMake config files cho integration

2. **Samples**
   - Tất cả samples được build sẽ được include
   - Basic samples, RKNN samples, TensorRT samples (nếu enabled)
   - MQTT samples (nếu CVEDIX_WITH_MQTT=ON)

3. **Data Directory**
   - `cvedix_data/` sẽ được include nếu tồn tại trong source directory
   - Chứa models, configs, và các file data cần thiết

4. **Documentation**
   - Release notes: `RELEASE_NOTES.md`
   - API documentation trong header files

