# Hướng dẫn đóng gói Debian Package

## Tổng quan

Dự án này hỗ trợ đóng gói thành Debian package (.deb) có thể cài đặt trên các hệ thống Debian/Ubuntu.

## Có 2 phương pháp build

### Phương pháp 1: Sử dụng CPack (CMake) - Đơn giản hơn

```bash
./build_deb_package.sh
```

Hoặc với các tùy chọn:

```bash
./build_deb_package.sh --version=2025.0.1.2 --build-type=Release --clean
```

### Phương pháp 2: Sử dụng debuild/dpkg-buildpackage - Chuẩn Debian

```bash
cd debian
./build_deb.sh
```

Hoặc với các tùy chọn:

```bash
cd debian
./build_deb.sh --clean --release
```

## Yêu cầu

### Dependencies cần cài đặt:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    debhelper \
    dpkg-dev \
    cmake (>= 3.10) \
    pkg-config \
    libopencv-dev (>= 4.6) \
    libgstreamer1.0-dev (>= 1.14.5) \
    libgstreamer-plugins-base1.0-dev (>= 1.14.5) \
    libgstrtspserver-1.0-dev \
    git
```

## Cấu trúc Package

Sau khi build, sẽ tạo ra 2 packages:

1. **cvedix-ai-runtime**: Runtime package
   - Libraries: `/usr/lib/`
   - Binaries: `/usr/bin/`
   - Data: `/usr/bin/cvedix_data/`
   - Documentation: `/usr/share/doc/cvedix-ai-runtime/`

2. **cvedix-ai-runtime-dev**: Development package
   - Headers: `/usr/include/cvedix/`
   - CMake config: `/usr/lib/cmake/cvedix/`
   - pkg-config: `/usr/lib/pkgconfig/`

## Cài đặt Package

### Cài đặt từ .deb file:

```bash
sudo dpkg -i cvedix-ai-runtime_*.deb cvedix-ai-runtime-dev_*.deb
sudo apt-get install -f  # Fix dependencies nếu cần
```

### Hoặc sử dụng apt:

```bash
sudo apt-get install ./cvedix-ai-runtime_*.deb ./cvedix-ai-runtime-dev_*.deb
```

## Kiểm tra Package

### Xem thông tin package:

```bash
dpkg -I cvedix-ai-runtime_*.deb
```

### Xem nội dung package:

```bash
dpkg -c cvedix-ai-runtime_*.deb
```

### Kiểm tra dependencies:

```bash
dpkg -I cvedix-ai-runtime_*.deb | grep Depends
```

## Sử dụng sau khi cài đặt

### CMake

```cmake
find_package(cvedix REQUIRED)
target_link_libraries(your_app PRIVATE cvedix::cvedix_instance_sdk)
```

CMake sẽ tự động tìm package tại `/usr/lib/cmake/cvedix/`

### pkg-config

pkg-config sẽ tự động tìm package trong `/usr/lib/pkgconfig/` (standard path)

```bash
g++ main.cpp $(pkg-config --cflags --libs cvedix) -o my_app
```

### Runtime

Thư viện được cài đặt vào `/usr/lib/` (standard path), hệ thống sẽ tự động tìm thấy.
Nếu cần, có thể chạy:

```bash
sudo ldconfig
```

## Gỡ cài đặt

```bash
sudo apt-get remove cvedix-ai-runtime cvedix-ai-runtime-dev
```

## Troubleshooting

### Lỗi dependencies

Nếu gặp lỗi dependencies khi cài đặt:

```bash
sudo apt-get install -f
```

### Lỗi build

1. Kiểm tra tất cả build dependencies đã được cài đặt
2. Kiểm tra log build để xem lỗi cụ thể
3. Thử clean build: `./build_deb_package.sh --clean`

### Lỗi runtime

1. Chạy `ldconfig` để cập nhật library cache
2. Kiểm tra libraries có tồn tại: `ls -la /usr/lib/libcvedix*`
3. Kiểm tra binaries: `ls -la /usr/bin/cvedix*`

## Notes

- Package được cài đặt vào `/usr` (standard Debian location)
- RKNN và RGA support tự động được bật cho ARM architectures (arm64, armhf)
- Package version được lấy từ `debian/changelog`
- Libraries và binaries được cài đặt vào standard system paths

