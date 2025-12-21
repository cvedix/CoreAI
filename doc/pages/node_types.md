/**
 * @page node_types Các loại Node
 *
 * @tableofcontents
 *
 * @section nt_overview Tổng quan
 *
 * Tất cả các Node trong framework đều kế thừa từ @ref cvedix_nodes::cvedix_node "cvedix_node".
 * Mỗi Node thực hiện một chức năng cụ thể trong pipeline.
 *
 * @section nt_source Source Nodes
 *
 * Đọc dữ liệu đầu vào và tạo @ref cvedix_objects::cvedix_frame_meta "frame metadata".
 *
 * | Node | Mô tả |
 * |------|-------|
 * | @ref cvedix_nodes::cvedix_file_src_node "cvedix_file_src_node" | Đọc từ file video |
 * | @ref cvedix_nodes::cvedix_rtsp_src_node "cvedix_rtsp_src_node" | Đọc từ RTSP stream |
 * | @ref cvedix_nodes::cvedix_rtmp_src_node "cvedix_rtmp_src_node" | Đọc từ RTMP stream |
 * | @ref cvedix_nodes::cvedix_udp_src_node "cvedix_udp_src_node" | Đọc từ UDP stream |
 * | @ref cvedix_nodes::cvedix_image_src_node "cvedix_image_src_node" | Đọc từ image file |
 *
 * @section nt_infer Infer Nodes
 *
 * Thực hiện suy luận AI để phát hiện/phân loại đối tượng.
 *
 * | Node | Backend | Mô tả |
 * |------|---------|-------|
 * | **Primary Infer** | OpenCV/TensorRT/ONNX | Phát hiện trên toàn frame |
 * | **Secondary Infer** | OpenCV/TensorRT/ONNX | Phân loại trên vùng crop |
 *
 * @subsection nt_infer_face Face Detection/Recognition
 * - YuNet Face Detector
 * - InsightFace Recognition
 * - FaceNet Embeddings
 *
 * @subsection nt_infer_object Object Detection
 * - YOLOv5/v7/v8
 * - SSD MobileNet
 *
 * @section nt_track Track Nodes
 *
 * Gán ID và theo dõi đối tượng qua các frame.
 *
 * | Node | Thuật toán |
 * |------|------------|
 * | @ref cvedix_nodes::cvedix_sort_track_node "cvedix_sort_track_node" | SORT (IOU-based) |
 * | @ref cvedix_nodes::cvedix_dsort_track_node "cvedix_dsort_track_node" | DeepSORT (Re-ID) |
 *
 * @section nt_ba Behavior Analysis Nodes
 *
 * Phân tích hành vi dựa trên tracking data.
 *
 * | Node | Sự kiện |
 * |------|---------|
 * | @ref cvedix_nodes::cvedix_ba_crossline_node "cvedix_ba_crossline_node" | Vượt đường kẻ |
 * | cvedix_ba_stop_node | Dừng/đỗ xe |
 * | cvedix_ba_jam_node | Ùn tắc |
 *
 * @section nt_broker Broker Nodes
 *
 * Xuất dữ liệu có cấu trúc ra bên ngoài.
 *
 * | Node | Đích |
 * |------|------|
 * | @ref cvedix_nodes::cvedix_msg_broker_node "cvedix_msg_broker_node" | Base class |
 * | cvedix_json_mqtt_broker_node | MQTT (JSON) |
 * | cvedix_json_kafka_broker_node | Kafka (JSON) |
 * | cvedix_json_console_broker_node | Console (JSON) |
 *
 * @section nt_osd OSD Nodes
 *
 * Vẽ overlay lên frame để hiển thị kết quả.
 *
 * - Vẽ bounding box
 * - Vẽ text label
 * - Vẽ polygon vùng quan tâm
 *
 * @see @ref architecture "Kiến trúc tổng quan"
 */
