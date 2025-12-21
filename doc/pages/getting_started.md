/**
 * @page getting_started Bắt đầu nhanh
 *
 * @tableofcontents
 *
 * @section gs_requirements Yêu cầu hệ thống
 *
 * | Thành phần | Phiên bản |
 * |------------|-----------|
 * | C++ | 17 trở lên |
 * | OpenCV | >= 4.6 |
 * | GStreamer | >= 1.14.5 |
 * | GCC | >= 7.5 |
 *
 * @section gs_install Cài đặt SDK
 *
 * @subsection gs_install_deb Từ gói .deb
 * @code{.bash}
 * sudo dpkg -i libcvedix-dev_*.deb
 * @endcode
 *
 * @subsection gs_install_verify Kiểm tra cài đặt
 * @code{.bash}
 * pkg-config --modversion cvedix
 * @endcode
 *
 * @section gs_cmake Tích hợp CMake
 *
 * Thêm vào file `CMakeLists.txt`:
 *
 * @code{.cmake}
 * find_package(cvedix REQUIRED)
 * target_link_libraries(my_app PRIVATE cvedix::cvedix_instance_sdk)
 * @endcode
 *
 * @section gs_example Ví dụ đơn giản
 *
 * @code{.cpp}
 * #include <cvedix/nodes/src/cvedix_file_src_node.h>
 * #include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
 * #include <cvedix/utils/analysis_board/cvedix_analysis_board.h>
 *
 * int main() {
 *     CVEDIX_LOGGER_INIT();
 *
 *     // 1. Tạo Source Node
 *     auto source = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
 *         "src", 0, "video.mp4", 1.0);
 *
 *     // 2. Tạo Inference Node
 *     auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
 *         "detector", "face_detection_yunet.onnx");
 *
 *     // 3. Link Pipeline
 *     detector->attach_to({source});
 *
 *     // 4. Chạy
 *     source->start();
 *
 *     // 5. Debug Visualizer
 *     cvedix_utils::cvedix_analysis_board board({source});
 *     board.display();
 *
 *     return 0;
 * }
 * @endcode
 *
 * @section gs_next Bước tiếp theo
 *
 * - Xem @ref node_types "Các loại Node" để hiểu chi tiết từng loại
 * - Xem @ref architecture "Kiến trúc" để nắm tổng quan hệ thống
 * - Xem @ref sdk_integration "Tích hợp SDK" cho các tùy chọn nâng cao
 */
