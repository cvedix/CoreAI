/**
 * @page release_notes Release Notes
 *
 * @tableofcontents
 *
 * @section rn_latest Phiên bản Mới Nhất
 *
 * **Version:** 2025.0.1.3  
 * **Release Date:** December 7, 2025
 *
 * ---
 *
 * @section rn_2025_0_1_3 v2025.0.1.3 (December 7, 2025)
 *
 * ### ✨ Tính năng mới
 *
 * | Tính năng | Mô tả |
 * |-----------|-------|
 * | **InsightFace Recognition** | Node nhận diện khuôn mặt ONNX-based, không cần TensorRT |
 * | **MLLM Analysis** | Phân tích ảnh với LLM (Ollama, OpenAI) |
 *
 * ### 🔧 Cải tiến
 * - RTSP Source: Tự động chọn decoder theo platform (mppvideodec/avdec_h264)
 * - OpenSSL dependency check với thông báo rõ ràng
 * - Kafka conditional compilation
 * - Auto-cleanup log files
 *
 * ### 🐛 Bug Fixes
 * - RTSP source platform incompatibility (AMD64)
 * - Kafka headers unconditional include
 * - OpenSSL error messages
 * - Image source node preprocessor guards
 *
 * ---
 *
 * @section rn_2025_0_1_2 v2025.0.1.2 (November 24, 2025)
 *
 * ### ✨ Tính năng mới
 *
 * | Tính năng | Mô tả |
 * |-----------|-------|
 * | **Custom Data Transform Node** | Tùy chỉnh frame_meta trước khi gửi broker |
 * | **Enhanced MQTT Support** | Cải thiện header verification |
 * | **Enhanced JSON Output** | Base64 encoded images, bbox formats |
 *
 * ### 🔧 Cải tiến
 * - MQTT header search logic
 * - Package config file generation
 * - Thêm sample `rknn_rtsp_tracking_mqtt_sample`
 *
 * ### 🐛 Bug Fixes
 * - MQTT header search
 * - Package config generation với relative paths
 *
 * ---
 *
 * @section rn_install Installation
 *
 * @subsection rn_install_deb Từ Debian Package
 *
 * @code{.bash}
 * # ARM64 (Rockchip, Jetson)
 * sudo dpkg -i cvedix-ai-runtime-2025.0.1.3-arm64.deb
 *
 * # x86_64
 * sudo dpkg -i cvedix-ai-runtime-2025.0.1.3-x86_64.deb
 *
 * # Fix dependencies
 * sudo apt-get install -f
 * @endcode
 *
 * @subsection rn_install_sdk Sử dụng SDK
 *
 * @code{.cmake}
 * find_package(cvedix REQUIRED)
 * target_link_libraries(your_app cvedix::cvedix_instance_sdk)
 * @endcode
 *
 * ---
 *
 * @section rn_support Hỗ trợ
 *
 * - **Email:** support@cvedix.com
 * - **Website:** https://www.cvedix.com
 *
 * @see @ref getting_started "Bắt đầu nhanh"
 * @see @ref sdk_integration "Tích hợp SDK"
 */
