/**
 * @page architecture Kiến trúc Framework
 *
 * @tableofcontents
 *
 * @section arch_overview Tổng quan
 *
 * CVEDIX AI Runtime được thiết kế theo kiến trúc **đường ống (pipeline)** hướng plugin.
 * Mỗi thành phần xử lý được gọi là **Node**, hoạt động độc lập và kết nối với nhau
 * thông qua cơ chế publish-subscribe.
 *
 * @section arch_pipeline Pipeline Flow
 *
 * ```
 * [Source Node] → [Infer Node] → [Track Node] → [BA Node] → [Broker Node]
 *       ↓              ↓              ↓             ↓             ↓
 *    Decode        Detect         Assign ID      Analyze      Publish
 * ```
 *
 * @section arch_components Các thành phần chính
 *
 * | Component | Mô tả | Ví dụ |
 * |-----------|-------|-------|
 * | **Source Nodes** | Đọc dữ liệu đầu vào | RTSP, RTMP, File, UDP |
 * | **Infer Nodes** | Suy luận AI | YOLOv8, YuNet, InsightFace |
 * | **Track Nodes** | Theo dõi đối tượng | SORT, DeepSORT |
 * | **BA Nodes** | Phân tích hành vi | Crossline, Stop, Jam |
 * | **Broker Nodes** | Xuất dữ liệu | MQTT, Kafka, Console |
 * | **OSD Nodes** | Hiển thị overlay | Text, Box, Polygon |
 *
 * @section arch_metadata Luồng Metadata
 *
 * Dữ liệu di chuyển trong pipeline dưới dạng **Metadata**:
 *
 * - @ref cvedix_objects::cvedix_frame_meta "cvedix_frame_meta" - Chứa frame image và targets
 * - @ref cvedix_objects::cvedix_frame_target "cvedix_frame_target" - Đối tượng phát hiện được
 * - @ref cvedix_objects::cvedix_control_meta "cvedix_control_meta" - Lệnh điều khiển
 *
 * @section arch_threading Multi-threading
 *
 * Mỗi Node chạy trong thread riêng, giao tiếp qua queue thread-safe.
 * Điều này đảm bảo hiệu suất cao và khả năng mở rộng.
 *
 * @see @ref node_types "Các loại Node"
 * @see @ref getting_started "Bắt đầu nhanh"
 */
