# Hướng dẫn đóng gói SDK thành .deb package

## Tổng quan

Dự án này đã được cấu hình để đóng gói thành Debian package (.deb) có thể cài đặt và sử dụng trên các hệ thống Debian/Ubuntu khác.

## Cấu trúc Debian package

Package được chia thành 2 phần:

1. **cvedix-instance-sdk** - Runtime package
   - Shared libraries (`.so` files)
   - Share files (documentation, info)

2. **cvedix-instance-sdk-dev** - Development package
   - Header files
   - CMake config files
   - pkg-config files

## Yêu cầu

Trước khi build package, cần cài đặt các công cụ sau:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    debhelper \
    dpkg-dev \
    cmake \
    pkg-config \
    libopencv-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstrtspserver-1.0-dev \
    git
```

## Cách build .deb package

### Cách 1: Sử dụng script tự động (Khuyến nghị)

```bash
# Build package với version mặc định
./build_deb.sh

# Build với version tùy chỉnh
./build_deb.sh --version=1.0.1

# Build cho architecture cụ thể
./build_deb.sh --arch=arm64

# Xem tất cả tùy chọn
./build_deb.sh --help
```

### Cách 2: Sử dụng dpkg-buildpackage trực tiếp

```bash
# Build package
dpkg-buildpackage -b -us -uc

# Package sẽ được tạo trong thư mục cha
```

## Cấu trúc file sau khi build

Sau khi build thành công, các file sau sẽ được tạo trong thư mục cha:

```
../
├── cvedix-instance-sdk_1.0.0-1_amd64.deb      # Runtime package
├── cvedix-instance-sdk-dev_1.0.0-1_amd64.deb  # Development package
└── cvedix-instance-sdk_1.0.0-1_amd64.buildinfo
```

## Cài đặt package

### Trên hệ thống đích

```bash
# Cài đặt cả runtime và dev packages
sudo dpkg -i cvedix-instance-sdk_*.deb

# Hoặc sử dụng apt
sudo apt-get install ./cvedix-instance-sdk_*.deb

# Nếu có dependency issues, fix với:
sudo apt-get install -f
```

### Cài đặt từ repository

Nếu bạn có APT repository:

```bash
# Thêm repository
echo "deb [trusted=yes] file:///path/to/packages ./" | sudo tee /etc/apt/sources.list.d/cvedix.list

# Update và cài đặt
sudo apt-get update
sudo apt-get install cvedix-instance-sdk cvedix-instance-sdk-dev
```

## Sử dụng SDK sau khi cài đặt

### Sử dụng CMake

```cmake
find_package(cvedix REQUIRED)
target_link_libraries(your_app PRIVATE cvedix::cvedix_instance_sdk)
```

### Sử dụng pkg-config

```bash
g++ main.cpp $(pkg-config --cflags --libs cvedix) -o my_app
```

## Gỡ cài đặt

```bash
# Gỡ cài đặt packages
sudo apt-get remove cvedix-instance-sdk cvedix-instance-sdk-dev

# Hoặc
sudo dpkg -r cvedix-instance-sdk cvedix-instance-sdk-dev
```

## Kiểm tra package

### Kiểm tra nội dung package

```bash
# Xem nội dung package
dpkg -c cvedix-instance-sdk_1.0.0-1_amd64.deb

# Xem thông tin package
dpkg -I cvedix-instance-sdk_1.0.0-1_amd64.deb
```

### Kiểm tra sau khi cài đặt

```bash
# Kiểm tra libraries
ldconfig -p | grep cvedix

# Kiểm tra pkg-config
pkg-config --modversion cvedix
pkg-config --cflags --libs cvedix

# Kiểm tra CMake
cmake --find-package -DNAME=cvedix -DCOMPILER_ID=GNU -DLANGUAGE=CXX
```

## Di chuyển package sang hệ thống khác

### Yêu cầu trên hệ thống đích

- Ubuntu/Debian tương thích (tương tự hệ thống build)
- OpenCV >= 4.5
- GStreamer >= 1.14.5
- Các dependencies khác (xem `debian/control`)

### Các bước

1. Copy các file `.deb` sang hệ thống đích
2. Cài đặt dependencies:
   ```bash
   sudo apt-get update
   sudo apt-get install -f
   ```
3. Cài đặt packages:
   ```bash
   sudo dpkg -i cvedix-instance-sdk_*.deb
   sudo apt-get install -f  # Fix dependencies nếu cần
   ```
4. Cập nhật library cache:
   ```bash
   sudo ldconfig
   ```

## Troubleshooting

### Lỗi: "dpkg-buildpackage: error"

- Kiểm tra tất cả dependencies đã được cài đặt
- Kiểm tra quyền ghi trong thư mục build
- Xem log chi tiết trong `debian/build.log`

### Lỗi: "dh: command not found"

```bash
sudo apt-get install debhelper
```

### Lỗi: "Missing dependencies"

Kiểm tra `debian/control` và cài đặt các Build-Depends:
```bash
sudo apt-get build-dep cvedix-instance-sdk
```

### Package không tìm thấy libraries sau khi cài đặt

```bash
# Cập nhật library cache
sudo ldconfig

# Kiểm tra LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
```

## Tùy chỉnh package

### Thay đổi version

Sửa file `debian/changelog` hoặc dùng `dch`:
```bash
dch -i  # Tăng version
dch -v 1.0.1  # Set version cụ thể
```

### Thay đổi dependencies

Sửa file `debian/control` trong phần `Depends:` và `Build-Depends:`

### Thêm files vào package

Sửa file `debian/rules` trong phần `override_dh_auto_install`

## Tạo repository APT

Nếu muốn tạo APT repository để phân phối:

```bash
# Tạo repository structure
mkdir -p repo/pool/main/c/cvedix-instance-sdk
cp *.deb repo/pool/main/c/cvedix-instance-sdk/

# Tạo Packages file
cd repo
dpkg-scanpackages pool/ > Packages
gzip -k Packages

# Tạo Release file (nếu cần)
apt-ftparchive release . > Release
```

## Lưu ý

- Package được build cho architecture hiện tại
- Để build cho architecture khác, cần cross-compilation setup
- Kiểm tra dependencies trên hệ thống đích trước khi phân phối
- Version trong `debian/changelog` phải match với version trong `CMakeLists.txt`

