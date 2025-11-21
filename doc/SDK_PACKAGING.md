# Tài liệu đóng gói SDK

## Tổng quan

Dự án này đã được cấu hình để đóng gói thành một SDK C++ hoàn chỉnh, có thể dễ dàng tích hợp vào các dự án khác.

## Các file đã được tạo/cập nhật

### 1. Scripts đóng gói
- **`build_sdk.sh`** - Script chính để build và đóng gói SDK với nhiều tùy chọn
- **`package.sh`** - Script wrapper đơn giản hơn (tương thích ngược)

### 2. Cấu hình CMake
- **`CMakeLists.txt`** - Đã được cập nhật để:
  - Cài đặt pkg-config file
  - Export CMake targets đúng cách
  - Cài đặt header files có cấu trúc
  - Tạo config files cho CMake

- **`cmake/cvedix-config.cmake.in`** - Template cho CMake config
- **`cmake/cvedix.pc.in`** - Template cho pkg-config file

### 3. Tài liệu
- **`README_SDK.md`** - Hướng dẫn chi tiết về cách sử dụng SDK
- **`INSTALL_SDK.md`** - Hướng dẫn cài đặt SDK
- **`SDK_PACKAGING.md`** - File này, tài liệu về quá trình đóng gói

### 4. Ví dụ
- **`examples/example_using_sdk/`** - Ví dụ về cách sử dụng SDK trong dự án mới

## Cách sử dụng

### Build SDK

```bash
# Cách đơn giản nhất
./build_sdk.sh

# Với các tùy chọn
./build_sdk.sh --prefix=/opt/cvedix --build-type=Release --with-cuda --with-trt

# Xem tất cả tùy chọn
./build_sdk.sh --help
```

### Cấu trúc SDK sau khi build

```
output/
├── include/
│   └── cvedix/              # Tất cả header files
│       ├── nodes/
│       ├── objects/
│       ├── utils/
│       └── cvedix_version.h
├── lib/
│   ├── libcvedix_instance_sdk.so
│   ├── cmake/
│   │   └── cvedix/          # CMake config files
│   └── pkgconfig/
│       └── cvedix.pc        # pkg-config file
└── share/
    └── cvedix/
        └── sdk_info.txt
```

## Tích hợp vào dự án khác

### Sử dụng CMake (Khuyến nghị)

```cmake
find_package(cvedix REQUIRED)
target_link_libraries(my_app PRIVATE cvedix::cvedix_instance_sdk)
```

### Sử dụng pkg-config

```bash
g++ main.cpp $(pkg-config --cflags --libs cvedix) -o my_app
```

Xem `README_SDK.md` để biết thêm chi tiết.

## Các tính năng của SDK

1. **CMake Integration** - Hỗ trợ `find_package()` đầy đủ
2. **pkg-config Support** - Hỗ trợ pkg-config cho các build system khác
3. **Version Information** - Thông tin phiên bản được embed trong SDK
4. **Structured Headers** - Header files được tổ chức rõ ràng
5. **Documentation** - Tài liệu đầy đủ và ví dụ

## Lưu ý

- SDK được build với `-fPIC` để có thể link vào shared libraries
- Tất cả dependencies phải được cài đặt trước khi build SDK
- SDK có thể được build với các tính năng tùy chọn (CUDA, TensorRT, etc.)

## Troubleshooting

Nếu gặp vấn đề khi build SDK:

1. Kiểm tra dependencies: OpenCV, GStreamer
2. Kiểm tra quyền ghi vào thư mục install
3. Xem log chi tiết trong `build_sdk/CMakeCache.txt`
4. Thử build với `--build-type=Debug` để có thông tin debug

## Tương lai

Có thể mở rộng thêm:
- [ ] Tạo tarball/zip package tự động
- [ ] Hỗ trợ Debian/RPM packages
- [ ] CI/CD để tự động build và release SDK
- [ ] Versioning tự động từ Git tags

