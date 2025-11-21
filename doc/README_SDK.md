# CVEDIX Instance Pipeline SDK - Hướng dẫn sử dụng

## Tổng quan

CVEDIX Instance Pipeline SDK là một framework C++ để phân tích và xử lý video, được đóng gói dưới dạng SDK để dễ dàng tích hợp vào các dự án của bạn.

## Cài đặt SDK

### Cách 1: Sử dụng build script (Khuyến nghị)

```bash
# Build và cài đặt SDK với cấu hình mặc định
./build_sdk.sh

# Hoặc chỉ định thư mục cài đặt
./build_sdk.sh --prefix=/usr/local

# Build với các tính năng tùy chọn
./build_sdk.sh --with-cuda --with-trt --build-type=Release

# Xem tất cả các tùy chọn
./build_sdk.sh --help
```

### Cách 2: Sử dụng CMake trực tiếp

```bash
mkdir -p build_sdk
cd build_sdk
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/path/to/install \
    -DCMAKE_BUILD_TYPE=Release \
    -DCVEDIX_WITH_CUDA=OFF \
    -DCVEDIX_WITH_TRT=OFF
make -j$(nproc)
make install
```

Sau khi cài đặt, SDK sẽ được đặt tại thư mục bạn chỉ định (mặc định là `./output`).

## Cấu trúc SDK

Sau khi cài đặt, SDK có cấu trúc như sau:

```
output/
├── include/
│   └── cvedix/              # Header files
│       ├── nodes/          # Node headers
│       ├── objects/        # Object definitions
│       ├── utils/          # Utility headers
│       └── cvedix_version.h
├── lib/
│   ├── libcvedix_instance_sdk.so    # Shared library
│   ├── cmake/
│   │   └── cvedix/         # CMake config files
│   │       ├── cvedix-config.cmake
│   │       ├── cvedix-config-version.cmake
│   │       └── cvedix-targets.cmake
│   └── pkgconfig/
│       └── cvedix.pc      # pkg-config file
└── share/
    └── cvedix/
        └── sdk_info.txt   # SDK information
```

## Sử dụng SDK trong dự án của bạn

### Cách 1: Sử dụng CMake (Khuyến nghị)

Tạo file `CMakeLists.txt` trong dự án của bạn:

```cmake
cmake_minimum_required(VERSION 3.10)
project(my_project)

set(CMAKE_CXX_STANDARD 17)

# Tìm SDK (có thể chỉ định đường dẫn nếu không cài đặt hệ thống)
set(CMAKE_PREFIX_PATH "/path/to/sdk/install" ${CMAKE_PREFIX_PATH})
find_package(cvedix REQUIRED)

# Tạo executable
add_executable(my_app main.cpp)

# Liên kết với SDK
target_link_libraries(my_app PRIVATE cvedix::cvedix_instance_sdk)
```

Ví dụ code `main.cpp`:

```cpp
#include <cvedix/cvedix_version.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/infers/cvedix_yolo_detector_node.h>
#include <cvedix/utils/analysis_board/cvedix_analysis_board.h>

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();
    
    // Tạo nodes
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src_0", 0, "./test_video.mp4", 0.6);
    
    auto yolo_detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector", 
        "./models/yolov3.weights",
        "./models/yolov3.cfg",
        "./models/classes.txt");
    
    // Kết nối pipeline
    yolo_detector->attach_to({file_src});
    
    // Khởi động
    file_src->start();
    
    // Debug
    cvedix_utils::cvedix_analysis_board board({file_src});
    board.display(1, false);
    
    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    
    return 0;
}
```

### Cách 2: Sử dụng pkg-config

```bash
# Compile với pkg-config
g++ -std=c++17 main.cpp \
    $(pkg-config --cflags --libs cvedix) \
    -o my_app
```

Nếu SDK không được cài đặt hệ thống, chỉ định đường dẫn:

```bash
export PKG_CONFIG_PATH=/path/to/sdk/install/lib/pkgconfig:$PKG_CONFIG_PATH
pkg-config --cflags --libs cvedix
```

### Cách 3: Sử dụng trực tiếp (Manual)

```cmake
cmake_minimum_required(VERSION 3.10)
project(my_project)

set(CMAKE_CXX_STANDARD 17)

# Đường dẫn đến SDK
set(CVEDIX_SDK_ROOT "/path/to/sdk/install")

# Include directories
include_directories(${CVEDIX_SDK_ROOT}/include)

# Link directories
link_directories(${CVEDIX_SDK_ROOT}/lib)

# Tạo executable
add_executable(my_app main.cpp)

# Liên kết
target_link_libraries(my_app PRIVATE cvedix_instance_sdk)
```

## Yêu cầu hệ thống

### Phụ thuộc bắt buộc
- **C++17** compiler (GCC >= 7.5)
- **OpenCV >= 4.6**
- **GStreamer >= 1.14.5**

### Phụ thuộc tùy chọn
- **CUDA** (nếu build với `--with-cuda`)
- **TensorRT** (nếu build với `--with-trt`)
- **PaddlePaddle** (nếu build với `--with-paddle`)
- **Kafka** (nếu build với `--with-kafka`)
- **OpenSSL** (nếu build với `--with-llm`)
- **FFmpeg** (nếu build với `--with-ffmpeg`)
- **RKNN** (nếu build với `--with-rknn`)
- **RGA** (nếu build với `--with-rga`)

## Kiểm tra cài đặt

Sau khi cài đặt SDK, bạn có thể kiểm tra bằng cách:

```bash
# Kiểm tra CMake config
cmake --find-package -DNAME=cvedix -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST

# Kiểm tra pkg-config
pkg-config --modversion cvedix
pkg-config --cflags cvedix
pkg-config --libs cvedix

# Kiểm tra thư viện
ldconfig -p | grep cvedix
```

## Ví dụ sử dụng

Xem thư mục `samples/` để biết các ví dụ chi tiết về cách sử dụng SDK.

Một số ví dụ phổ biến:
- `rtsp_ba_crossline_sample.cpp` - Phân tích hành vi với RTSP input
- `file_src_sample.cpp` - Đọc video từ file
- `yolo_detector_sample.cpp` - Phát hiện đối tượng với YOLO

## Troubleshooting

### Lỗi: "Could not find cvedix"
- Đảm bảo SDK đã được cài đặt đúng cách
- Kiểm tra `CMAKE_PREFIX_PATH` hoặc `PKG_CONFIG_PATH`
- Xác nhận đường dẫn trong `CMakeLists.txt` của bạn

### Lỗi: "undefined reference"
- Đảm bảo đã link với `cvedix::cvedix_instance_sdk`
- Kiểm tra thứ tự link libraries
- Đảm bảo tất cả dependencies đã được cài đặt

### Lỗi runtime: "cannot open shared object file"
- Kiểm tra `LD_LIBRARY_PATH` có chứa đường dẫn đến thư viện
- Chạy `ldconfig` nếu cài đặt hệ thống
- Hoặc copy thư viện vào thư mục system library

## Tài liệu thêm

- [README.md](./README.md) - Tài liệu chính của dự án
- [SAMPLES.md](./SAMPLES.md) - Danh sách các ví dụ
- [nodes/README.md](./nodes/README.md) - Tài liệu về các nodes

## Hỗ trợ

Nếu gặp vấn đề, vui lòng:
1. Kiểm tra tài liệu và examples
2. Xem các issues trên GitHub
3. Tạo issue mới với thông tin chi tiết về lỗi

