# Hướng dẫn cài đặt CVEDIX SDK

## Cài đặt nhanh

```bash
# 1. Clone repository
git clone <repository-url>
cd instance_pipeline

# 2. Build và cài đặt SDK
./build_sdk.sh

# 3. SDK sẽ được cài đặt vào ./output
```

## Cài đặt vào hệ thống

```bash
# Cài đặt vào /usr/local (yêu cầu quyền root)
sudo ./build_sdk.sh --prefix=/usr/local

# Sau khi cài đặt, cập nhật library cache
sudo ldconfig
```

## Cài đặt vào thư mục tùy chỉnh

```bash
# Cài đặt vào thư mục tùy chỉnh
./build_sdk.sh --prefix=/opt/cvedix

# Thêm vào PATH và LD_LIBRARY_PATH
export PATH=/opt/cvedix/bin:$PATH
export LD_LIBRARY_PATH=/opt/cvedix/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=/opt/cvedix/lib/pkgconfig:$PKG_CONFIG_PATH
```

## Cấu hình biến môi trường

Thêm vào `~/.bashrc` hoặc `~/.profile`:

```bash
# CVEDIX SDK
export CVEDIX_SDK_ROOT=/opt/cvedix
export PATH=$CVEDIX_SDK_ROOT/bin:$PATH
export LD_LIBRARY_PATH=$CVEDIX_SDK_ROOT/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$CVEDIX_SDK_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH
export CMAKE_PREFIX_PATH=$CVEDIX_SDK_ROOT:$CMAKE_PREFIX_PATH
```

Sau đó chạy:
```bash
source ~/.bashrc
```

## Kiểm tra cài đặt

```bash
# Kiểm tra thư viện
ldconfig -p | grep cvedix

# Kiểm tra pkg-config
pkg-config --modversion cvedix

# Kiểm tra CMake
cmake --find-package -DNAME=cvedix -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST
```

## Gỡ cài đặt

Nếu cài đặt vào hệ thống:
```bash
sudo rm -rf /usr/local/lib/libcvedix_instance_sdk.so*
sudo rm -rf /usr/local/include/cvedix
sudo rm -rf /usr/local/lib/cmake/cvedix
sudo rm -rf /usr/local/lib/pkgconfig/cvedix.pc
sudo ldconfig
```

Nếu cài đặt vào thư mục tùy chỉnh:
```bash
rm -rf /opt/cvedix  # hoặc thư mục bạn đã chọn
```

