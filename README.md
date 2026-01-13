## 一、Giới thiệu

`AI Core Runtime` là một framework dùng để phân tích và cấu trúc hóa video, được viết bằng C++, ít phụ thuộc và dễ sử dụng. Nó hoạt động giống như một đường ống, trong đó mỗi nút độc lập với nhau và có thể tự kết hợp, `AI Core Runtime` có thể được sử dụng để xây dựng các ứng dụng phân tích video khác nhau, phù hợp với các tình huống như cấu trúc hóa video, tìm kiếm hình ảnh, nhận dạng khuôn mặt, phân tích hành vi trong lĩnh vực giao thông/an ninh (như phát hiện sự kiện giao thông), v.v.

## 二、Ưu điểm và đặc điểm

`AI Core Runtime` tương tự như framework DeepStream của NVIDIA và mxVision của Huawei, nhưng dễ sử dụng hơn và có tính di động cao hơn.

`AI Core Runtime` sử dụng phong cách mã hóa hướng plugin, có thể kết hợp theo nhu cầu khác nhau, chúng ta có thể sử dụng các plugin độc lập (tức là kiểu `Node` trong framework) để xây dựng các ứng dụng phân tích video khác nhau. Bạn chỉ cần chuẩn bị mô hình và hiểu cách phân tích đầu ra của nó, suy luận có thể dựa trên các backend khác nhau, chẳng hạn như OpenCV::DNN (mặc định), TensorRT, PaddleInference, ONNXRuntime, v.v., bất kỳ cái nào bạn thích.

## 四、Tính năng

AI Core Runtime là một framework giúp tích hợp mô hình thuật toán thị giác máy tính trở nên đơn giản hơn, lưu ý rằng nó không phải là framework học sâu như TensorFlow, TensorRT. Các tính năng chính của AI Core Runtime như sau:

- Đọc luồng: Hỗ trợ các giao thức video phổ biến như udp, rtsp, rtmp, file, application. Đồng thời hỗ trợ đọc hình ảnh.
- Giải mã video: Hỗ trợ giải mã video và hình ảnh dựa trên OpenCV/GStreamer (hỗ trợ tăng tốc phần cứng).
- Suy luận thuật toán: Hỗ trợ suy luận đa cấp dựa trên thuật toán học sâu, chẳng hạn như phát hiện đối tượng, phân loại hình ảnh, trích xuất đặc trưng, tạo hình ảnh và các mạng liên quan khác. Đồng thời hỗ trợ tích hợp thuật toán hình ảnh truyền thống. **Hỗ trợ tích hợp mô hình đa phương thức lớn (mLLM) (cập nhật 2025/8/12)**
- Theo dõi đối tượng: Hỗ trợ theo dõi đối tượng, chẳng hạn như thuật toán theo dõi IOU, SORT, v.v.
- Phân tích hành vi (BA): Hỗ trợ phân tích hành vi dựa trên theo dõi, chẳng hạn như vượt đường, đỗ xe, vi phạm giao thông và các hành vi giao thông khác.
- Logic nghiệp vụ: Hỗ trợ tích hợp logic nghiệp vụ tùy chỉnh bất kỳ, có thể liên quan chặt chẽ đến nghiệp vụ.
- Proxy dữ liệu: Hỗ trợ đẩy dữ liệu có cấu trúc (json/xml/định dạng tùy chỉnh) lên đám mây, tệp hoặc nền tảng bên thứ ba khác thông qua kafka/Socket, v.v.
- Ghi lại: Hỗ trợ ghi video trong khoảng thời gian cụ thể, chụp ảnh khung cụ thể và lưu vào tệp.
- Hiển thị màn hình (OSD): Hỗ trợ vẽ dữ liệu có cấu trúc và kết quả xử lý logic nghiệp vụ lên khung hình.
- Mã hóa video: Hỗ trợ mã hóa video và hình ảnh dựa trên OpenCV/GStreamer (hỗ trợ tăng tốc phần cứng).
- Đẩy luồng: Hỗ trợ các giao thức video phổ biến như udp, rtsp, rtmp, file, application. Đồng thời hỗ trợ đẩy hình ảnh.

## 五、Bắt đầu nhanh

### 5.1 Phụ thuộc

Nền tảng

- Ubuntu 18.04 x86_64 NVIDIA rtx/tesla GPUs
- Ubuntu 18.04 aarch64 NVIDIA jetson serials device，tx2 tested
- Ubuntu 22.04 x86_64 by VMware virtual machine on Windows 10, pure CPUs
- Ubuntu 18.04 x86_64 Cambrian MLU serials device, MLU 370 tested
- Ubuntu 18.04 aarch64 Rockchip RK35** serials device, RK3588 tested
- Ubuntu 22.04 aarch64 Ascend 310/910 serials device, Atlas 300I-Pro tested
- Chờ bạn kiểm tra

Cơ bản

- C++ 17
- OpenCV >= 4.6
- GStreamer 1.14.5 (Required by OpenCV)
- GCC >= 7.5

Tùy chọn, nếu bạn cần triển khai backend suy luận của riêng mình hoặc sử dụng backend suy luận khác ngoài `opencv::dnn`.

- CUDA
- TensorRT
- Paddle Inference
- ONNX Runtime
- mLLM（Ollama/vLLM/OpenAI-compatible API Services）
- Bất kỳ thứ gì bạn thích

[Cách cài đặt CUDA và TensorRT](./third_party/trt_vehicle/README.md)

[Cách cài đặt Paddle_Inference](./third_party/paddle_ocr/README.md)

### 5.2 Cài đặt nhanh Dependencies

Khi clone repository sang máy mới, sử dụng các lệnh sau để cài đặt dependencies tự động theo phần cứng:

```bash
# Cài đặt dependencies (interactive - hỏi về optional deps)
make setup

# Hoặc cài đặt tự động (chỉ base deps, không hỏi)
make setup-auto
```

**Các lệnh Make có sẵn:**

| Lệnh | Mô tả |
|------|-------|
| `make setup` | Cài đặt dependencies (tự động detect hardware) |
| `make setup-auto` | Cài đặt base dependencies (non-interactive) |
| `make build` | Build với auto-detect hardware |
| `make build-cpu` | Build cho CPU only |
| `make build-rockchip` | Build cho Rockchip RK35xx |
| `make package-cpu` | Tạo .deb package cho CPU |
| `make package-rockchip` | Tạo .deb package cho Rockchip |
| `make info` | Hiển thị thông tin hardware đã detect |
| `make clean` | Xóa build directories |

Script `setup_dependencies.sh` sẽ tự động:

- Phát hiện kiến trúc (x86_64, aarch64)
- Phát hiện platform (Rockchip, Jetson, NVIDIA GPU, CPU-only)
- Cài đặt dependencies phù hợp với phần cứng

### 5.3 Cài đặt & Tích hợp SDK

Thay vì biên dịch toàn bộ mã nguồn framework, chúng tôi khuyến nghị sử dụng SDK đã đóng gói để phát triển ứng dụng.

#### 1. Cài đặt SDK

Nếu bạn đã có gói `.deb`:

```bash
sudo dpkg -i libcvedix-dev_*.deb
```

Hoặc build và cài đặt SDK từ source (nếu chưa có gói pre-built):

```bash
./build_sdk.sh --prefix=/usr/local
```

#### 2. Tích hợp vào dự án CMake

Trong file `CMakeLists.txt` của dự án bạn:

```cmake
# Tìm gói cvedix
find_package(cvedix REQUIRED)

# Link thư viện (Tự động bao gồm cả đường dẫn include)
target_link_libraries(my_app PRIVATE cvedix::cvedix_instance_sdk)

# Lưu ý: Không cần thêm include_directories() vì target đã chứa sẵn thông tin này.
```

#### 3. Ví dụ đơn giản (Minimal Example)

```cpp
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
#include <cvedix/utils/analysis_board/cvedix_analysis_board.h>

int main() {
    CVEDIX_LOGGER_INIT();

    // 1. Tạo Source Node
    auto source = std::make_shared<cvedix_nodes::cvedix_file_src_node>("src", 0, "video.mp4", 1.0);
    
    // 2. Tạo Inference Node
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("detector", "face_detection_yunet.onnx");

    // 3. Link Pipeline
    detector->attach_to({source});

    // 4. Chạy
    source->start();

    // 5. Debug Visualizer
    cvedix_utils::cvedix_analysis_board board({source});
    board.display();
    
    return 0;
}
```

Để xem thêm các ví dụ nâng cao (RTMP, OSD, Face Recognition...), vui lòng xem mục [Các mẫu nguyên mẫu](#55-các-mẫu-nguyên-mẫu).

### 5.5 Các mẫu nguyên mẫu

Tổng cộng hơn 40 mẫu nguyên mẫu, [Nhấp vào](./SAMPLES.md) để xem thêm.

## 六、Tài liệu thêm

- [How AI Core Runtime Works](./doc/about.md)
- [Development Environment For Reference](./doc/env.md)

## 七、Triển khai Thực tế & Đóng gói

Để triển khai dự án vào môi trường sản xuất (Production), chúng tôi cung cấp các công cụ đóng gói SDK và ứng dụng thành các gói cài đặt chuẩn (như `.deb` cho Debian/Ubuntu) để dễ dàng phân phối và cài đặt.

### 1. Quy trình đóng gói

Chúng tôi hỗ trợ đóng gói nhị phân cho cả kiến trúc x86_64 và aarch64 (như NVIDIA Jetson, Rockchip).

- **Đóng gói SDK**: Tạo gói `.deb` chứa thư viện (`.so`), header files, và các công cụ hỗ trợ phát triển.
  - Script: `build_cpu_deb_package.sh` (chỉ CPU) hoặc các script tương ứng cho GPU/NPU.
  - Gói đầu ra: `libcvedix-dev_<version>_<arch>.deb`
  
- **Đóng gói Ứng dụng/Runtime**: Tạo gói chứa runtime (thư viện động) và các ứng dụng mẫu/thực thi, cùng với mô hình AI cần thiết.
  - Script: `build_package_with_models.sh`
  - Tự động bao gồm các mô hình từ `cvedix_data`.

### 2. Cài đặt và Sử dụng

Sau khi đóng gói, việc cài đặt trên máy đích rất đơn giản:

```bash
# Cài đặt gói .deb
sudo dpkg -i libcvedix-dev_*.deb

# Kiểm tra cài đặt
pkg-config --modversion cvedix
```

Sau khi cài đặt, SDK sẽ nằm trong hệ thống (thường là `/usr/lib` và `/usr/include`), cho phép bạn phát triển ứng dụng mới hoặc chạy ứng dụng đã biên dịch mà không cần thiết lập lại môi trường build phức tạp.

### 3. Tùy chọn triển khai

- **Docker**: Bạn có thể sử dụng các gói `.deb` này để xây dựng Docker image nhỏ gọn cho ứng dụng của mình.
- **Service**: Tích hợp với `systemd` để chạy ứng dụng như một dịch vụ nền (background service), tự động khởi động cùng hệ thống.
