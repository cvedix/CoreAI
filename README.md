## 一、Giới thiệu

`InstancePipeline` là một framework dùng để phân tích và cấu trúc hóa video, được viết bằng C++, ít phụ thuộc và dễ sử dụng. Nó hoạt động giống như một đường ống, trong đó mỗi nút độc lập với nhau và có thể tự kết hợp, `InstancePipeline` có thể được sử dụng để xây dựng các ứng dụng phân tích video khác nhau, phù hợp với các tình huống như cấu trúc hóa video, tìm kiếm hình ảnh, nhận dạng khuôn mặt, phân tích hành vi trong lĩnh vực giao thông/an ninh (như phát hiện sự kiện giao thông), v.v.

## 二、Ưu điểm và đặc điểm

`InstancePipeline` tương tự như framework DeepStream của NVIDIA và mxVision của Huawei, nhưng dễ sử dụng hơn và có tính di động cao hơn.

`InstancePipeline` sử dụng phong cách mã hóa hướng plugin, có thể kết hợp theo nhu cầu khác nhau, chúng ta có thể sử dụng các plugin độc lập (tức là kiểu `Node` trong framework) để xây dựng các ứng dụng phân tích video khác nhau. Bạn chỉ cần chuẩn bị mô hình và hiểu cách phân tích đầu ra của nó, suy luận có thể dựa trên các backend khác nhau, chẳng hạn như OpenCV::DNN (mặc định), TensorRT, PaddleInference, ONNXRuntime, v.v., bất kỳ cái nào bạn thích.

## 四、Tính năng

InstancePipeline là một framework giúp tích hợp mô hình thuật toán thị giác máy tính trở nên đơn giản hơn, lưu ý rằng nó không phải là framework học sâu như TensorFlow, TensorRT. Các tính năng chính của InstancePipeline như sau:

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
- Ubuntu 18.04 x86_64 Cambrian MLU serials device, MLU 370 tested (code not provided)
- Ubuntu 18.04 aarch64 Rockchip RK35** serials device, RK3588 tested (code not provided)
- Ubuntu 22.04 aarch64 Ascend 310/910 serials device, Atlas 300I-Pro tested (code not provided)
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

### 5.2 Biên dịch và gỡ lỗi

1. Chạy `git clone <>`
2. Chạy `cd InstancePipeline`
3. Chạy `mkdir build && cd build`
4. Chạy `cmake ..`
5. Chạy `make -j8`

Sau khi biên dịch xong, tất cả các tệp thư viện được lưu trong `build/libs`, tất cả các tệp chạy Sample được lưu trong `build/bin`. Khi thực hiện bước 4, bạn có thể thêm một số tùy chọn biên dịch:
- -DCVEDIX_WITH_CUDA=ON （Biên dịch các chức năng liên quan đến CUDA, mặc định là OFF）
- -DCVEDIX_WITH_TRT=ON （Biên dịch các chức năng và Samples liên quan đến TensorRT, mặc định là OFF）
- -DCVEDIX_WITH_PADDLE=ON （Biên dịch các chức năng và Samples liên quan đến PaddlePaddle, mặc định là OFF）
- -DCVEDIX_WITH_KAFKA=ON （Biên dịch các chức năng và Samples liên quan đến Kafka, mặc định là OFF）
- -DCVEDIX_WITH_LLM=ON （Biên dịch các chức năng và Samples liên quan đến LLM, mặc định là OFF）
- -DCVEDIX_BUILD_COMPLEX_SAMPLES=ON （Biên dịch các Samples nâng cao, mặc định là OFF）

Ví dụ, nếu cần bật các mô-đun liên quan đến CUDA và TensorRT, bạn có thể chạy `cmake -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_TRT=ON ..`. Nếu chỉ chạy `cmake ..`, thì tất cả mã sẽ chạy trên CPU.

```
# Bật tất cả
cmake -DCVEDIX_WITH_CUDA=ON \
-DCVEDIX_WITH_TRT=ON \
-DCVEDIX_WITH_PADDLE=ON \
-DCVEDIX_WITH_KAFKA=ON \
-DCVEDIX_BUILD_COMPLEX_SAMPLES=ON ..

# Tắt tất cả (mặc định)
cmake ..
```

Nếu muốn chạy các Samples đã biên dịch, trước tiên hãy tải xuống các tệp mô hình và dữ liệu kiểm tra:

1. [Tải xuống tệp kiểm tra và mô hình từ Google Drive](https://drive.google.com/drive/folders/1v9dVcR6xttUTB-WPsH3mZ_ZZMzD4wG-v?usp=sharing)
2. [Tải xuống tệp kiểm tra và mô hình từ Baidu Netdisk](https://pan.baidu.com/s/1jr2nBnEDmuNaM5DiMjbC0g?pwd=nf53)

Đặt thư mục đã tải xuống (tên là cvedix_data) ở bất kỳ vị trí nào (ví dụ: đặt trong `/root/abc`), sau đó chạy Sample trong `cùng thư mục`, ví dụ: thực thi lệnh trong `/root/abc`: `[path to InstancePipeline]/build/bin/1-1-1_sample` để chạy 1-1-1_sample.

**Lưu ý**：`./third_party/` bên dưới đều là các dự án độc lập, một số là thư viện header-only, được InstancePipeline trực tiếp tham chiếu; một số chứa tệp cpp, có thể biên dịch hoặc chạy độc lập, InstancePipeline phụ thuộc vào các thư viện này, trong quá trình biên dịch InstancePipeline sẽ tự động biên dịch các thư viện này. Các thư viện này cũng chứa Samples của riêng chúng, cách sử dụng cụ thể có thể tham khảo tệp README trong thư mục con tương ứng.

### 5.3 Cách sử dụng

1. Trước tiên biên dịch InstancePipeline thành thư viện, sau đó tham chiếu nó.
2. Hoặc trực tiếp tham chiếu mã nguồn, sau đó biên dịch toàn bộ Application.

Dưới đây là một Sample về cách xây dựng Pipeline rồi chạy (vui lòng sửa đổi đường dẫn tệp liên quan trong mã trước):

```c++
#include "../nodes/src/cvedix_file_src_node.h"
#include "../nodes/infers/cvedix_yunet_face_detector_node.h"
#include "../nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "../nodes/osd/cvedix_face_osd_node_v2.h"
#include "../nodes/des/cvedix_screen_des_node.h"
#include "../nodes/des/cvedix_rtmp_des_node.h"
#include "../utils/analysis_board/cvedix_analysis_board.h"

/*
* Tên：1-1-N sample
* Mã đầy đủ nằm tại：samples/1-1-N_sample.cpp
* Mô tả chức năng：1 đầu vào video, 1 tác vụ phân tích video (phát hiện và nhận dạng khuôn mặt), 2 đầu ra (đầu ra màn hình/đầu ra đẩy luồng RTMP)
*/

int main() {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    // 1、Tạo nút
    // Node lấy video
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./test_video/10.mp4", 0.6);
    // 2、Node suy luận mô hình
    // Suy luận cấp một：Phát hiện khuôn mặt
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", "./models/face/face_detection_yunet_2022mar.onnx");
    // Suy luận cấp hai：Nhận dạng khuôn mặt
    auto sface_face_encoder_0 = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_0", "./models/face/face_recognition_sface_2021dec.onnx");
    // 3、Node OSD
    // Vẽ kết quả xử lý lên khung hình
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd_0");
    // Hiển thị màn hình
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);
    // Hiển thị đẩy luồng
    auto rtmp_des_0 = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>("rtmp_des_0", 0, "rtmp://192.168.77.60/live/10000");

    // Xây dựng đường ống, liên kết kết quả xử lý của các nút
    yunet_face_detector_0->attach_to({file_src_0});
    sface_face_encoder_0->attach_to({yunet_face_detector_0});
    osd_0->attach_to({sface_face_encoder_0});

    // Đường ống tự động tách, xuất kết quả qua màn hình/đẩy luồng
    screen_des_0->attach_to({osd_0});
    rtmp_des_0->attach_to({osd_0});

    // Khởi động đường ống
    file_src_0->start();

    // Trực quan hóa đường ống
    cvedix_utils::cvedix_analysis_board board({file_src_0});
    board.display();
}
```
#### Node phát hiện khuôn mặt Yunet INT8 mới

Đối với các trường hợp muốn chạy trực tiếp YuNet INT8 trên CPU (OpenCV FaceDetectorYN), bạn có thể sử dụng node mới `cvedix_face_yunet_int8_face_detection_mode`. Node này mặc định tải mô hình:

```
./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx
```

Ví dụ sử dụng:

```c++
auto yunet_int8 = std::make_shared<cvedix_nodes::cvedix_face_yunet_int8_face_detection_mode>(
    "yunet_face_int8",
    "./cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx",
    0.9f,   // score threshold
    0.3f,   // nms threshold
    5000,   // top_k
    320,
    320);

yunet_int8->attach_to({file_src_0});
```

Node này sử dụng `cv::FaceDetectorYN` nên không yêu cầu RKNN và có thể chạy tốt trên CPU thông thường.

Sau khi chạy mã trên, sẽ xuất hiện 3 màn hình:
1. Biểu đồ trạng thái chạy của đường ống, trạng thái tự động làm mới
2. Kết quả hiển thị màn hình (GUI)
3. Kết quả hiển thị trình phát (RTMP)

### 5.4 Các mẫu nguyên mẫu

Tổng cộng hơn 40 mẫu nguyên mẫu, [Nhấp vào](./SAMPLES.md) để xem thêm.

## 六、Tài liệu thêm

- [How InstancePipeline Works](./doc/about.md)
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