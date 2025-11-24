# CVEDIX AI Runtime SDK - Release Notes v2025.0.1.2

**Release Date:** November 24, 2025  
**Version:** 2025.0.1.2

---

## 🎉 Tổng quan

Phiên bản 2025.0.1.2 của CVEDIX AI Runtime SDK mang đến nhiều cải tiến quan trọng, bao gồm hỗ trợ MQTT broker, custom data transformation node, và các cải tiến về JSON output với base64 encoded images.

---

## ✨ Tính năng mới

### 1. Custom Data Transform Node
- **Node mới:** `cvedix_custom_data_transform_node`
- Cho phép tùy chỉnh dữ liệu `frame_meta` trước khi gửi đến broker
- Hỗ trợ filter targets theo nhiều điều kiện (score, class_id, track_id)
- Có thể thêm/sửa/xóa thông tin trong targets
- Flexible với callback function để customize theo yêu cầu khách hàng

**Ví dụ sử dụng:**
```cpp
auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        // Filter targets với score >= 0.3
        // Custom logic here...
        return meta;
    }
);
```

### 2. Enhanced MQTT Support
- **Cải thiện:** MQTT header verification
  - Header search luôn được thực hiện, kể cả khi pkg-config thành công
  - Đảm bảo `mosquitto.h` tồn tại trước khi compile
- **Cải thiện:** Package config generation
  - Sử dụng `configure_package_config_file` thay vì `configure_file`
  - Hỗ trợ proper package-relative path resolution
  - `CVEDIX_WITH_MQTT` được thêm vào config defines

### 3. Enhanced JSON Output
- Base64 encoded crop images cho mỗi target
- Bounding box coordinates với nhiều format (x1, y1, x2, y2, center_x, center_y)
- Secondary class information
- Customizable JSON structure thông qua transformer function

---

## 🔧 Cải tiến

### Build System
- **Cải thiện:** MQTT header search logic
  - Luôn verify header tồn tại, không phụ thuộc vào pkg-config
  - Better error handling và fallback mechanism
- **Cải thiện:** Package config file generation
  - Proper handling của `@PACKAGE_INIT@` variable
  - Support cho package-relative paths

### Samples
- **Mới:** `rknn_rtsp_tracking_mqtt_sample` với custom transform node
  - Ví dụ đầy đủ về pipeline: RTSP → RKNN Detector → Tracker → Custom Transform → MQTT Broker
  - Demo filter targets và custom data transformation

---

## 📦 Package Contents

Package bao gồm:

### Libraries
- `libcvedix_instance_sdk.so` - Main SDK library
- CMake config files trong `/opt/cvedix/lib/cmake/cvedix/`
- pkg-config file trong `/opt/cvedix/lib/pkgconfig/`

### Headers
- Tất cả header files trong `/opt/cvedix/include/cvedix/`
- Bao gồm:
  - Nodes (src, des, mid, infer, track, osd, broker)
  - Objects (frame_meta, targets, etc.)
  - Utils (logger, mqtt_client, analysis_board, etc.)

### Samples
- Basic samples (1-1-1, 1-N-N, ba_crossline, etc.)
- RKNN samples (rknn_detector_sample, rknn_rtsp_tracking_mqtt_sample, etc.)
- TensorRT samples (nếu build với CVEDIX_WITH_TRT=ON)
- MQTT samples (nếu build với CVEDIX_WITH_MQTT=ON)

### Data
- `cvedix_data/` directory với models và configs (nếu có)
- Được cài đặt vào `/opt/cvedix/bin/cvedix_data/`

---

## 🐛 Bug Fixes

### Bug 1: MQTT Header Search
- **Vấn đề:** Header search chỉ xảy ra khi pkg-config fail
- **Fix:** Header verification luôn được thực hiện, bất kể pkg-config có thành công hay không
- **Impact:** Tránh compilation errors khi header không tồn tại

### Bug 2: Package Config Generation
- **Vấn đề:** Sử dụng `configure_file` thay vì `configure_package_config_file`
- **Fix:** Chuyển sang `configure_package_config_file` với proper INSTALL_DESTINATION
- **Impact:** Package config files hoạt động đúng với relative paths

---

## 📋 Dependencies

### Bắt buộc
- OpenCV >= 4.6
- GStreamer >= 1.14.5
  - gstreamer1.0-plugins-base
  - gstreamer1.0-plugins-good
  - gstreamer1.0-plugins-bad
  - libgstreamer1.0-0
  - libgstreamer-plugins-base1.0-0

### Tùy chọn
- **RKNN:** rknn-toolkit2 (cho Rockchip NPU)
- **MQTT:** libmosquitto-dev (cho MQTT support)
- **TensorRT:** TensorRT libraries (cho NVIDIA GPU)
- **CUDA:** CUDA toolkit (cho GPU acceleration)
- **PaddlePaddle:** PaddlePaddle libraries (cho Paddle inference)

---

## 🚀 Installation

### Cài đặt từ Debian Package

Package được đóng gói riêng cho từng architecture:

```bash
# Cài đặt package cho ARM64
sudo dpkg -i cvedix-ai-runtime-2025.0.1.2-arm64.deb

# Cài đặt package cho x86_64
sudo dpkg -i cvedix-ai-runtime-2025.0.1.2-x86_64.deb

# Hoặc sử dụng apt
sudo apt-get install ./cvedix-ai-runtime-2025.0.1.2-arm64.deb

# Fix dependencies nếu cần
sudo apt-get install -f
```

**Lưu ý:** Chọn đúng package cho architecture của hệ thống:
- **ARM64** (aarch64): `cvedix-ai-runtime-2025.0.1.2-arm64.deb`
- **x86_64**: `cvedix-ai-runtime-2025.0.1.2-x86_64.deb`

### Sử dụng SDK

```cmake
# Trong CMakeLists.txt của project
find_package(cvedix REQUIRED)
target_link_libraries(your_target cvedix::cvedix_instance_sdk)
```

---

## 📝 Migration Notes

### Từ version trước

1. **MQTT Support:**
   - Nếu đã sử dụng MQTT, không cần thay đổi code
   - Header verification được cải thiện tự động

2. **Custom Data Transform:**
   - Node mới, không ảnh hưởng code hiện tại
   - Có thể thêm vào pipeline để customize data

3. **Package Config:**
   - Nếu sử dụng `find_package(cvedix)`, sẽ hoạt động tốt hơn với relative paths

---

## 🔍 Known Issues

- Không có known issues trong phiên bản này

---

## 📚 Documentation

- **API Documentation:** Xem header files trong `/opt/cvedix/include/cvedix/`
- **Samples:** Xem source code trong `/opt/cvedix/bin/` hoặc `samples/` directory
- **Custom Transform Node:** Xem `nodes/mid/README_CUSTOM_DATA_TRANSFORM.md`
- **Packaging Guide:** Xem `README_PACKAGING.md`

---

## 👥 Contributors

CVEDIX Development Team

---

## 📞 Support

- **Email:** support@cvedix.com
- **Website:** https://www.cvedix.com

---

## 📄 License

Proprietary - CVEDIX

---

**Changelog:**
- v2025.0.1.2 (2025-11-24)
  - Added Custom Data Transform Node
  - Improved MQTT header verification
  - Fixed package config generation
  - Enhanced JSON output with base64 images
  - Updated samples with MQTT and custom transform examples

