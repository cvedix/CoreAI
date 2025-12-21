/**
 * @page sdk_integration Tích hợp SDK
 *
 * @tableofcontents
 *
 * @section sdk_overview Tổng quan
 *
 * CVEDIX AI Runtime SDK cung cấp thư viện và headers để tích hợp vào ứng dụng của bạn
 * mà không cần biên dịch lại toàn bộ framework.
 *
 * @section sdk_install Cài đặt
 *
 * @subsection sdk_install_deb Từ gói Debian
 *
 * @code{.bash}
 * # Cài đặt
 * sudo dpkg -i libcvedix-dev_2025.0.1_amd64.deb
 *
 * # Kiểm tra
 * pkg-config --modversion cvedix
 * @endcode
 *
 * @subsection sdk_install_paths Đường dẫn cài đặt
 *
 * | Loại | Đường dẫn |
 * |------|-----------|
 * | Headers | `/usr/include/cvedix/` |
 * | Libraries | `/usr/lib/` |
 * | CMake Config | `/usr/lib/cmake/cvedix/` |
 *
 * @section sdk_cmake Tích hợp CMake
 *
 * @subsection sdk_cmake_basic Cơ bản
 *
 * @code{.cmake}
 * cmake_minimum_required(VERSION 3.16)
 * project(my_app)
 *
 * find_package(cvedix REQUIRED)
 *
 * add_executable(my_app main.cpp)
 * target_link_libraries(my_app PRIVATE cvedix::cvedix_instance_sdk)
 * @endcode
 *
 * @subsection sdk_cmake_optional Với các tùy chọn
 *
 * @code{.cmake}
 * # Tìm với TensorRT support
 * find_package(cvedix REQUIRED COMPONENTS tensorrt)
 *
 * # Hoặc với ONNX Runtime
 * find_package(cvedix REQUIRED COMPONENTS onnxruntime)
 * @endcode
 *
 * @section sdk_includes Include Headers
 *
 * @code{.cpp}
 * // Core nodes
 * #include <cvedix/nodes/common/cvedix_node.h>
 * #include <cvedix/nodes/src/cvedix_file_src_node.h>
 * #include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
 *
 * // Inference nodes
 * #include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
 *
 * // Tracking
 * #include <cvedix/nodes/track/cvedix_sort_track_node.h>
 *
 * // Objects/Metadata
 * #include <cvedix/objects/cvedix_frame_meta.h>
 * #include <cvedix/objects/cvedix_frame_target.h>
 *
 * // Utilities
 * #include <cvedix/utils/cvedix_logger.h>
 * #include <cvedix/utils/analysis_board/cvedix_analysis_board.h>
 * @endcode
 *
 * @section sdk_backends Inference Backends
 *
 * SDK hỗ trợ nhiều backend suy luận:
 *
 * | Backend | Macro định nghĩa | Ghi chú |
 * |---------|------------------|---------|
 * | OpenCV DNN | Mặc định | Không cần cấu hình thêm |
 * | TensorRT | `WITH_TENSORRT` | Cần CUDA + TensorRT |
 * | ONNX Runtime | `WITH_ONNXRUNTIME` | Cross-platform |
 * | Paddle Inference | `WITH_PADDLE` | OCR, NLP |
 *
 * @section sdk_samples Samples
 *
 * Xem thư mục `samples/` để có các ví dụ đầy đủ:
 *
 * - `samples/1-1-N_sample.cpp` - Multi-camera inference
 * - `samples/face_recognition_sample.cpp` - Face detection + recognition
 * - `samples/ba_crossline_sample.cpp` - Crossline detection
 *
 * @see @ref getting_started "Bắt đầu nhanh"
 * @see @ref node_types "Các loại Node"
 */
