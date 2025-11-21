# Hướng dẫn đóng gói SDK thành .deb package

## Tổng quan

Dự án này đã được cấu hình để đóng gói thành Debian package (.deb) có thể cài đặt và sử dụng trên các hệ thống Debian/Ubuntu khác.

## Quick Start

### 1. Cài đặt dependencies

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

### 2. Build .deb package

```bash
./build_deb.sh
```

### 3. Cài đặt package

```bash
cd ..
sudo dpkg -i cvedix-instance-sdk_*.deb
sudo apt-get install -f  # Fix dependencies nếu cần
```

## Chi tiết

Xem file `doc/DEB_PACKAGING.md` để biết hướng dẫn chi tiết.

## Cấu trúc package

- **cvedix-instance-sdk**: Runtime package (libraries)
- **cvedix-instance-sdk-dev**: Development package (headers, CMake config)

## Sử dụng sau khi cài đặt

### CMake
```cmake
find_package(cvedix REQUIRED)
target_link_libraries(your_app PRIVATE cvedix::cvedix_instance_sdk)
```

### pkg-config
```bash
g++ main.cpp $(pkg-config --cflags --libs cvedix) -o my_app
```

