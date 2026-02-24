# Danh mục Nodes & Hướng dẫn Samples

## Mục lục

- [Phần I: Danh mục Node](#phần-i-danh-mục-node)
- [Phần II: Hướng dẫn Samples](#phần-ii-hướng-dẫn-samples)

---

# Phần I: Danh mục Node

## 1. Source Nodes (`nodes/src/`)

Source node là **điểm bắt đầu** của pipeline, chịu trách nhiệm đọc dữ liệu video/ảnh và tạo `frame_meta`.

| Node | Header | Mô tả | Constructor Parameters |
|------|--------|--------|----------------------|
| `cvedix_file_src_node` | [cvedix_file_src_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/src/cvedix_file_src_node.h) | Đọc video từ file | `(name, channel, file_path, resize_ratio)` |
| `cvedix_rtsp_src_node` | [cvedix_rtsp_src_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/src/cvedix_rtsp_src_node.h) | Đọc stream RTSP | `(name, channel, rtsp_url, resize_ratio)` |
| `cvedix_rtmp_src_node` | [cvedix_rtmp_src_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/src/cvedix_rtmp_src_node.h) | Đọc stream RTMP | `(name, channel, rtmp_url, resize_ratio)` |
| `cvedix_udp_src_node` | [cvedix_udp_src_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/src/cvedix_udp_src_node.h) | Đọc stream UDP | `(name, channel, udp_url, resize_ratio)` |
| `cvedix_image_src_node` | [cvedix_image_src_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/src/cvedix_image_src_node.h) | Đọc ảnh từ file/thư mục | `(name, channel, pattern, interval, resize_ratio)` |
| `cvedix_app_src_node` | [cvedix_app_src_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/src/cvedix_app_src_node.h) | Nhận frame từ application code | `(name, channel, resize_ratio)` |

> [!NOTE]
> Mỗi source node **bắt buộc** có `channel_index` riêng. Không thể `attach_to()` source node.

---

## 2. Destination Nodes (`nodes/des/`)

Destination node là **điểm kết thúc** của pipeline, tiêu thụ `frame_meta` và xuất kết quả.

| Node | Header | Mô tả | Constructor Parameters |
|------|--------|--------|----------------------|
| `cvedix_screen_des_node` | [cvedix_screen_des_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/des/cvedix_screen_des_node.h) | Hiển thị lên màn hình (OpenCV window) | `(name, channel)` |
| `cvedix_file_des_node` | [cvedix_file_des_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/des/cvedix_file_des_node.h) | Lưu video ra file | `(name, channel, output_path, ...)` |
| `cvedix_rtmp_des_node` | [cvedix_rtmp_des_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/des/cvedix_rtmp_des_node.h) | Đẩy video sang RTMP server | `(name, channel, rtmp_url)` |
| `cvedix_rtsp_des_node` | [cvedix_rtsp_des_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/des/cvedix_rtsp_des_node.h) | Đẩy video dạng RTSP (tự host) | `(name, channel, port, ...)` |
| `cvedix_image_des_node` | [cvedix_image_des_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/des/cvedix_image_des_node.h) | Lưu frame thành ảnh (file/socket) | `(name, channel, ...)` |
| `cvedix_app_des_node` | [cvedix_app_des_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/des/cvedix_app_des_node.h) | Đẩy frame về application code | `(name, channel)` |
| `cvedix_fake_des_node` | [cvedix_fake_des_node.h](file:///home/cvedix/Documents/Github/edgeos-sdk/nodes/des/cvedix_fake_des_node.h) | Không làm gì (cho benchmarking) | `(name, channel)` |

> [!NOTE]
> Mỗi destination node **bắt buộc** có `channel_index`. Không thể có node tiếp theo sau destination.

---

## 3. Inference Nodes (`nodes/infers/`)

Inference node thực hiện **suy luận AI** theo pipeline 4 bước: `prepare → preprocess → infer → postprocess`.

### 3.1 Base Classes (`infers/base/`)

| Class | Mô tả |
|-------|--------|
| `cvedix_infer_node` | Lớp cơ sở, định nghĩa 4-step pipeline. Backend mặc định: OpenCV DNN |
| `cvedix_primary_infer_node` | Suy luận trên **toàn bộ frame** → tạo `targets[]` mới |
| `cvedix_secondary_infer_node` | Suy luận trên **từng target crop** → bổ sung `secondary_*` vào target |
| `cvedix_node_factory` | Factory pattern để tạo node từ config |
| `cvedix_inference_interface` | Interface chung cho inference |

### 3.2 Object Detection (Primary)

| Node | Model | Backend | Mô tả |
|------|-------|---------|--------|
| `cvedix_yolo_detector_node` | YOLOv3/v3-tiny | OpenCV DNN | Phát hiện đối tượng đa lớp |
| `cvedix_yolov11_detector_node` | YOLOv11 | OpenCV DNN | Phát hiện đối tượng (ONNX) |
| `cvedix_trt_yolov8_detector` | YOLOv8 | TensorRT | Phát hiện đối tượng GPU |
| `cvedix_trt_vehicle_detector` | YOLOv5s (vehicle) | TensorRT | Phát hiện phương tiện |
| `cvedix_trt_vehicle_scanner` | YOLOv5s (body) | TensorRT | Quét thân xe |
| `cvedix_rknn_yolov11_detector_node` | YOLOv11 | RKNN | Phát hiện trên Rockchip NPU |
| `cvedix_mask_rcnn_detector_node` | Mask R-CNN | OpenCV DNN | Instance segmentation |

### 3.3 Face Detection (Primary)

| Node | Model | Backend | Mô tả |
|------|-------|---------|--------|
| `cvedix_yunet_face_detector_node` | YuNet | OpenCV DNN | Phát hiện khuôn mặt (lightweight) |
| `cvedix_face_yunet_int8_face_detection_mode` | YuNet INT8 | OpenCV DNN | Phát hiện khuôn mặt (quantized) |
| `cvedix_trt_yolov11_face_detector_node` | YOLOv11-face | TensorRT | Phát hiện khuôn mặt GPU |
| `cvedix_rknn_face_detector_node` | YuNet/SCRFD | RKNN | Phát hiện khuôn mặt Rockchip |
| `cvedix_rknn_yolov8_detector_node` | YOLOv8-face | RKNN | Face trên Rockchip NPU |

### 3.4 Plate Detection (Primary)

| Node | Model | Backend | Mô tả |
|------|-------|---------|--------|
| `cvedix_yolov11_plate_detector_node` | YOLOv11 | OpenCV DNN | Phát hiện biển số |
| `cvedix_trt_yolov11_plate_detector_node` | YOLOv11 | TensorRT | Phát hiện biển số GPU |
| `cvedix_trt_vehicle_plate_detector` | YOLOv5s | TensorRT | Phát hiện + OCR biển số (2-stage) |
| `cvedix_trt_vehicle_plate_detector_v2` | YOLOv5s | TensorRT | Phát hiện + OCR biển số (1-stage) |

### 3.5 Classification (Secondary)

| Node | Model | Backend | Mô tả |
|------|-------|---------|--------|
| `cvedix_classifier_node` | ResNet series | OpenCV DNN | Phân loại hình ảnh chung |
| `cvedix_trt_vehicle_color_classifier` | ResNet18 | TensorRT | Phân loại màu xe |
| `cvedix_trt_vehicle_type_classifier` | ResNet18 | TensorRT | Phân loại loại xe |
| `cvedix_trt_yolov8_classifier` | YOLOv8-cls | TensorRT | Phân loại đa mục đích |

### 3.6 Feature Extraction (Secondary)

| Node | Model | Backend | Mô tả |
|------|-------|---------|--------|
| `cvedix_feature_encoder_node` | ResNet series | OpenCV DNN | Trích xuất feature vector |
| `cvedix_sface_feature_encoder_node` | SFace | OpenCV DNN | Trích xuất đặc trưng khuôn mặt |
| `cvedix_trt_vehicle_feature_encoder` | FastReID | TensorRT | Trích xuất đặc trưng xe |

### 3.7 Face Recognition (`infers/fr/`)

| Node | Backend | Mô tả |
|------|---------|--------|
| `cvedix_face_recognition_node` | OpenCV DNN | Nhận dạng khuôn mặt (InsightFace/ArcFace) |
| `cvedix_face_recognition_ort_node` | ONNX Runtime | Nhận dạng khuôn mặt (ONNX) |
| `cvedix_face_recognition_trt_node` | TensorRT | Nhận dạng khuôn mặt GPU |
| `cvedix_face_registration_node` | — | Đăng ký khuôn mặt mới |

### 3.8 Pose & Segmentation (Primary)

| Node | Model | Backend | Mô tả |
|------|-------|---------|--------|
| `cvedix_openpose_detector_node` | OpenPose | OpenCV DNN | Phát hiện tư thế cơ thể |
| `cvedix_trt_yolov8_pose_detector` | YOLOv8-pose | TensorRT | Phát hiện tư thế GPU |
| `cvedix_enet_seg_node` | ENet | OpenCV DNN | Semantic segmentation |
| `cvedix_trt_yolov8_seg_detector` | YOLOv8-seg | TensorRT | Instance segmentation GPU |

### 3.9 Others

| Node | Mô tả |
|------|--------|
| `cvedix_lane_detector_node` | Phát hiện làn đường (CenterNet) |
| `cvedix_restoration_node` | Nâng cấp/khôi phục hình ảnh (Real-ESRGAN) |
| `cvedix_facenet_node` | FaceNet face recognition |
| `cvedix_face_swap_node` | Thay thế khuôn mặt (InsightFace) |
| `cvedix_ppocr_text_detector_node` | Phát hiện văn bản (PaddleOCR) |
| `cvedix_plate_recogniton_ppocr3` | Nhận dạng biển số (PaddleOCR v3) |
| `cvedix_mllm_analyser_node` | Phân tích ảnh bằng LLM (Ollama/vLLM/OpenAI) |

---

## 4. Tracking Nodes (`nodes/track/`)

Tracking node gán **track_id** cho từng đối tượng và duy trì lịch sử di chuyển.

| Node | Thuật toán | Parameter đặc biệt | Mô tả |
|------|-----------|---------------------|--------|
| `cvedix_sort_track_node` | SORT | `track_for` (NORMAL/FACE) | Theo dõi bằng Kalman + IoU |
| `cvedix_bytetrack_node` | ByteTrack | `high/low_thresh, match_thresh` | Theo dõi high/low confidence |
| `cvedix_ocsort_track_node` | OC-SORT | `delta_t, asso_func, inertia` | Observation-centric SORT |
| `cvedix_dsort_track_node` | DeepSORT | — | SORT + appearance features |
| `cvedix_trt_botsort_track_node` | BoTSORT | — | ByteTrack + camera motion |

> [!TIP]
> `cvedix_track_for::FACE` sử dụng `face_targets[]` thay vì `targets[]` để theo dõi.

---

## 5. Behavior Analysis Nodes (`nodes/ba/`)

BA node phân tích **hành vi** dựa trên kết quả tracking, tạo `ba_results[]` và cập nhật `ba_flags`.

| Node | Hành vi | Input yêu cầu | Output |
|------|---------|---------------|--------|
| `cvedix_ba_crossline_node` | Vượt đường (đếm) | tracked targets + `cvedix_line` per channel | Đếm lên/xuống, sự kiện cross |
| `cvedix_ba_jam_node` | Tắc đường | tracked targets + region | Phát hiện mật độ cao |
| `cvedix_ba_stop_node` | Dừng xe bất thường | tracked targets | Phát hiện target đứng yên quá lâu |
| `cvedix_ba_loitering_node` | Lảng vảng | tracked targets + region | Phát hiện ở lại vùng quá lâu |
| `cvedix_ba_crowding_node` | Tập trung đông | tracked targets + region | Phát hiện đám đông |
| `cvedix_ba_area_enter_exit_node` | Vào/ra khu vực | tracked targets + polygon | Phát hiện đi vào/ra vùng |
| `cvedix_ba_line_counting` | Đếm qua nhiều đường | tracked targets + multiple lines | Đếm multi-line |

---

## 6. OSD Nodes (`nodes/osd/`)

OSD node **vẽ kết quả** phân tích lên frame, tạo `osd_frame` trong `frame_meta`.

| Node | Vẽ nội dung | Mô tả |
|------|-------------|--------|
| `cvedix_osd_node` | Bounding box + label | OSD cơ bản cho detection |
| `cvedix_osd_node_v2` | Bbox + label + sub_targets | Bao gồm đối tượng con |
| `cvedix_osd_node_v3` | Bbox + label + mask | Bao gồm segmentation mask |
| `cvedix_face_osd_node` | Face bbox + landmarks | OSD cho face detection |
| `cvedix_face_osd_node_v2` | Face bbox + similarity score | OSD face recognition |
| `cvedix_plate_osd_node` | Plate bbox + text | OSD biển số xe |
| `cvedix_pose_osd_node` | Skeleton keypoints | OSD tư thế cơ thể |
| `cvedix_seg_osd_node` | Segmentation overlay | OSD phân đoạn ảnh |
| `cvedix_text_osd_node` | Text bbox + OCR result | OSD nhận dạng văn bản |
| `cvedix_lane_osd_node` | Lane lines | OSD phát hiện làn đường |
| `cvedix_ba_crossline_osd_node` | Cross line + count | OSD vượt đường |
| `cvedix_ba_jam_osd_node` | Jam region highlight | OSD tắc đường |
| `cvedix_ba_stop_osd_node` | Stop indicator | OSD dừng xe |
| `cvedix_ba_crowding_osd_node` | Crowding region | OSD tập trung đông |
| `cvedix_ba_area_enter_exit_osd_node` | Area polygon + events | OSD vào/ra khu vực |
| `cvedix_mllm_osd_node` | LLM text description | OSD mô tả ảnh bằng LLM |
| `cvedix_cluster_node` | Cluster visualization | OSD phân cụm đối tượng |
| `cvedix_expr_osd_node` | Expression result | OSD kiểm tra biểu thức |

---

## 7. Broker Nodes (`nodes/broker/`)

Broker node **serialize và gửi** dữ liệu có cấu trúc ra bên ngoài pipeline.

| Node | Giao thức | Định dạng | Mô tả |
|------|-----------|-----------|--------|
| `cvedix_json_console_broker_node` | Console (stdout) | JSON | In kết quả ra terminal |
| `cvedix_json_enhanced_console_broker_node` | Console (stdout) | JSON (chi tiết) | In kết quả chi tiết hơn |
| `cvedix_json_mqtt_broker_node` | MQTT | JSON | Gửi qua MQTT broker |
| `cvedix_json_kafka_broker_node` | Kafka | JSON | Gửi qua Kafka |
| `cvedix_xml_file_broker_node` | File | XML | Lưu vào file XML |
| `cvedix_xml_socket_broker_node` | UDP Socket | XML | Gửi qua UDP |
| `cvedix_ba_socket_broker_node` | UDP Socket | Binary | Gửi kết quả BA |
| `cvedix_embeddings_socket_broker_node` | UDP Socket | Binary | Gửi feature embeddings |
| `cvedix_embeddings_properties_socket_broker_node` | UDP Socket | Binary | Gửi embeddings + thuộc tính |
| `cvedix_plate_socket_broker_node` | UDP Socket | Binary | Gửi kết quả biển số |
| `cvedix_expr_socket_broker_node` | UDP Socket | Binary | Gửi kết quả biểu thức |
| `cvedix_sse_broker_node` | HTTP SSE | JSON | Server-Sent Events (web) |
| `cvedix_msg_broker_node` | — | — | Lớp cơ sở cho mọi broker |

> [!TIP]
> `cvedix_broke_for` enum cho phép chọn loại dữ liệu serialize: `NORMAL`, `FACE`, `PLATE`, `BA`, `EMBEDDING`.

---

## 8. Middleware Nodes (`nodes/mid/`)

Middleware node **điều khiển luồng** dữ liệu trong pipeline.

| Node | Mô tả | Ví dụ sử dụng |
|------|--------|---------------|
| `cvedix_split_node` | Chia pipeline thành nhiều nhánh | Tách theo `channel_index` hoặc deep-copy |
| `cvedix_sync_node` | Đồng bộ nhiều nhánh pipeline | Hợp nhất kết quả từ nhiều inference |
| `cvedix_placeholder_node` | Node giữ chỗ (không xử lý) | Debug, testing |
| `cvedix_skip_node` | Bỏ qua frame (giảm tải) | Giảm FPS cho inference chậm |
| `cvedix_message_broker_node` | Lớp cơ sở cho broker (deprecated) | — |
| `cvedix_custom_data_transform_node` | Biến đổi dữ liệu tùy chỉnh | Giao diện callback linh hoạt |

### Split Node — Hai chế độ:

```cpp
// Chế độ 1: Split by channel index (mỗi nhánh nhận đúng channel của mình)
auto split = std::make_shared<cvedix_split_node>("split", true);  // by_channel = true

// Chế độ 2: Split by deep-copy (mỗi nhánh nhận bản sao)
auto split = std::make_shared<cvedix_split_node>("split", false, true);  // deep_copy = true
```

---

## 9. Record Node (`nodes/record/`)

| Node | Mô tả |
|------|--------|
| `cvedix_record_node` | Ghi video và chụp ảnh từ pipeline |
| `cvedix_record_task` | Task quản lý recording (base) |
| `cvedix_video_record_task` | Task ghi video |
| `cvedix_image_record_task` | Task chụp ảnh |

```cpp
// Constructor
auto recorder = std::make_shared<cvedix_record_node>(
    "recorder",
    "./record_video",   // thư mục lưu video
    "./record_image"    // thư mục lưu ảnh
);

// Hook khi recording hoàn thành
recorder->set_video_record_complete_hooker([](int channel, cvedix_record_info info) {
    std::cout << "Video saved: " << info.full_record_path << std::endl;
});
```

---

## 10. FFmpeg I/O Nodes (`nodes/ffio/`)

| Node | Mô tả |
|------|--------|
| `cvedix_ff_src_node` | Source node sử dụng FFmpeg (hardware decode) |
| `cvedix_ff_des_node` | Destination node sử dụng FFmpeg (hardware encode) |

> [!NOTE]
> Các node FFmpeg hỗ trợ tăng tốc phần cứng (NVDEC/NVENC) nếu có GPU NVIDIA.

---

## 11. Processing Nodes (`nodes/proc/`)

| Node | Mô tả |
|------|--------|
| `cvedix_frame_fusion_node` | Hợp nhất 2 frame thành 1 (side-by-side, overlay) |
| `cvedix_expr_check_node` | Kiểm tra biểu thức toán học trong ảnh |

---

## 12. Common / Base Classes (`nodes/common/`)

| Class | Mô tả |
|-------|--------|
| `cvedix_node` | **Lớp cơ sở** cho mọi node (dual-thread, pub/sub, hooks) |
| `cvedix_src_node` | Lớp cơ sở cho source nodes |
| `cvedix_des_node` | Lớp cơ sở cho destination nodes |
| `cvedix_meta_publisher` | Interface publish metadata sang subscribers |
| `cvedix_meta_subscriber` | Interface nhận metadata từ publisher |
| `cvedix_meta_hookable` | Interface hook 4 điểm giám sát |
| `cvedix_stream_info_hookable` | Hook lấy thông tin stream (width, height, fps) |
| `cvedix_stream_status_hookable` | Hook theo dõi trạng thái stream |
| `cvedix_license_node` | Kiểm tra license |
| `frame_utils` | Tiện ích xử lý frame (resize, prepare output) |

---

# Phần II: Hướng dẫn Samples

## 1. Cấu trúc chung của mọi Sample

Tất cả 122 samples đều tuân theo **mẫu 5 bước** nhất quán:

```cpp
#include "cvedix/nodes/src/..."    // 1. Include source node
#include "cvedix/nodes/infers/..."  //    Include inference nodes
#include "cvedix/nodes/osd/..."     //    Include OSD node
#include "cvedix/nodes/des/..."     //    Include destination node
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

int main() {
    // ═══════ Bước 1: Khởi tạo Logger ═══════
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // ═══════ Bước 2: Tạo Nodes ═══════
    auto src   = std::make_shared<cvedix_file_src_node>("src", 0, "video.mp4", 0.6);
    auto infer = std::make_shared<cvedix_yolo_detector_node>("det", "model.onnx", ...);
    auto osd   = std::make_shared<cvedix_osd_node>("osd");
    auto des   = std::make_shared<cvedix_screen_des_node>("des", 0);

    // ═══════ Bước 3: Kết nối Pipeline ═══════
    infer->attach_to({src});
    osd->attach_to({infer});
    des->attach_to({osd});

    // ═══════ Bước 4: Chạy Pipeline ═══════
    src->start();

    // ═══════ Bước 5: Debug & Chờ ═══════
    cvedix_utils::cvedix_analysis_board board({src});
    board.display(1, false);    // hiển thị FPS/latency

    std::string wait;
    std::getline(std::cin, wait);
    src->detach_recursively();  // cleanup
}
```

---

## 2. Phân loại Samples theo Chức năng

### 2.1 Pipeline cơ bản (Topology Samples)

Quy ước tên `X-Y-Z_sample`: **X** = số source, **Y** = số inference, **Z** = số output.

| Sample | Pipeline | Mô tả |
|--------|----------|--------|
| [1-1-1_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/1-1-1_sample.cpp) | `src → det → enc → osd → des` | 1 input, 1 task, 1 output |
| [1-1-N_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/1-1-N_sample.cpp) | `src → det → enc → osd → {des_0, des_1}` | 1 input, fan-out output |
| [1-N-1_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/1-N-1_sample.cpp) | `src → {det_a, det_b} → sync → osd → des` | 1 input, parallel inference |
| [1-N-N_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/1-N-N_sample.cpp) | `src → split → {branch_a, branch_b}` | 1 input split thành 2 nhánh |
| [N-1-N_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/N-1-N_sample.cpp) | `{src_0, src_1} → det → split → {des_0, des_1}` | Multi-channel chia sẻ detector |
| [N-N_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/N-N_sample.cpp) | `{src_0, src_1} → det → {des_0, des_1}` | Multi-channel basic |

```
Ví dụ topology 1-N-N:

                  ┌→ det_a → enc_a → osd_a → des_a
src → split(copy) ┤
                  └→ det_b → enc_b → osd_b → des_b
```

### 2.2 Source / Destination Samples

| Sample | Mô tả |
|--------|--------|
| [rtsp_src_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rtsp_src_sample.cpp) | Đọc từ RTSP camera |
| [rtmp_src_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rtmp_src_sample.cpp) | Đọc từ RTMP stream |
| [image_src_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/image_src_sample.cpp) | Đọc ảnh từ thư mục |
| [app_src_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/app_src_sample.cpp) | Nhận frame từ code |
| [app_src_des_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/app_src_des_sample.cpp) | App source + App destination |
| [rtsp_des_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rtsp_des_sample.cpp) | Output sang RTSP |
| [image_des_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/image_des_sample.cpp) | Output thành ảnh |
| [src_des_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/src_des_sample.cpp) | Source và Destination cơ bản |
| [app_des_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/app_des_sample.cpp) | Output về application |
| [ffmpeg_src_des_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ffmpeg_src_des_sample.cpp) | FFmpeg hardware codec |
| [ffmpeg_transcode_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ffmpeg_transcode_sample.cpp) | FFmpeg transcode |
| [nv_hard_codec_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/nv_hard_codec_sample.cpp) | NVIDIA hardware codec |

### 2.3 Face Detection & Recognition Samples

| Sample | Mô tả |
|--------|--------|
| [face_tracking_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_tracking_sample.cpp) | Phát hiện + theo dõi khuôn mặt (SORT) |
| [face_tracking_bytetrack_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_tracking_bytetrack_sample.cpp) | Face tracking (ByteTrack) |
| [face_tracking_ocsort_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_tracking_ocsort_sample.cpp) | Face tracking (OC-SORT) |
| [face_tracking_rtsp_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_tracking_rtsp_sample.cpp) | Face tracking từ RTSP (đầy đủ: detect + encode + track + OSD + MQTT) |
| [face_recognition_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_recognition_sample.cpp) | Nhận dạng khuôn mặt (InsightFace) |
| [face_recognition_ort_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_recognition_ort_sample.cpp) | Nhận dạng khuôn mặt (ONNX Runtime) |
| [face_recognition_mqtt_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_recognition_mqtt_sample.cpp) | Face recognition + MQTT broker |
| [face_registration_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_registration_sample.cpp) | Đăng ký khuôn mặt mới |
| [face_yunet_int8_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_yunet_int8_sample.cpp) | Face detection INT8 quantized |
| [face_swap_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/face_swap_sample.cpp) | Hoán đổi khuôn mặt |
| [facenet_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/facenet_sample.cpp) | FaceNet recognition |
| [facenet_register_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/facenet_register_sample.cpp) | FaceNet registration |
| [facenet_margin_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/facenet_margin_sample.cpp) | FaceNet with margin tuning |
| [insightface_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/insightface_sample.cpp) | InsightFace basic |
| [insightface_trt_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/insightface_trt_sample.cpp) | InsightFace TensorRT |
| [insightface_register_recognize_face_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/insightface_register_recognize_face_sample.cpp) | InsightFace register + recognize |
| [insightface_register_recognize_face_trt_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/insightface_register_recognize_face_trt_sample.cpp) | InsightFace TRT register + recognize |

### 2.4 Behavior Analysis Samples

| Sample | Hành vi | Pipeline |
|--------|---------|----------|
| [ba_crossline_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_crossline_sample.cpp) | Vượt đường | `src → det → tracker → ba_crossline → osd → des` |
| [ba_crossline_mqtt_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_crossline_mqtt_sample.cpp) | Vượt đường + MQTT | Crossline + MQTT broker |
| [ba_multiple_crossline_counting_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_multiple_crossline_counting_sample.cpp) | Đếm nhiều đường | Multiple lines counting |
| [ba_multiline_crossline_test.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_multiline_crossline_test.cpp) | Test multi-line | Testing multi-line crossline |
| [ba_jam_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_jam_sample.cpp) | Tắc đường | Jam detection |
| [ba_stop_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_stop_sample.cpp) | Dừng bất thường | Stop detection |
| [ba_loitering_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_loitering_sample.cpp) | Lảng vảng | Loitering detection |
| [ba_crowding_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_crowding_sample.cpp) | Tập trung đông | Crowding detection |
| [ba_area_enter_exit_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/ba_area_enter_exit_sample.cpp) | Vào/ra khu vực | Area enter/exit |
| [rtsp_ba_crossline_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rtsp_ba_crossline_sample.cpp) | Crossline từ RTSP | RTSP + behavior analysis |
| [wrong_way_detection_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/wrong_way_detection_sample.cpp) | Phát hiện đi ngược | Wrong way detection |

### 2.5 Vehicle & Plate Samples

| Sample | Mô tả |
|--------|--------|
| [vehicle_tracking_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/vehicle_tracking_sample.cpp) | Theo dõi phương tiện |
| [vehicle_body_scan_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/vehicle_body_scan_sample.cpp) | Quét thân xe |
| [vehicle_cluster_based_on_classify_encoding_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/vehicle_cluster_based_on_classify_encoding_sample.cpp) | Phân cụm xe theo đặc trưng |
| [body_scan_and_plate_detect_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/body_scan_and_plate_detect_sample.cpp) | Quét xe + phát hiện biển số |
| [plate_recognition_pipeline_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/plate_recognition_pipeline_sample.cpp) | Pipeline nhận dạng biển số |
| [plate_recognition_video_output_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/plate_recognition_video_output_sample.cpp) | Biển số + video output |
| [plate_recognize_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/plate_recognize_sample.cpp) | Nhận dạng biển số cơ bản |
| [plate_bytetrack_ocr_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/plate_bytetrack_ocr_sample.cpp) | Biển số + ByteTrack + OCR |
| [yolov11_plate_bytetrack_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/yolov11_plate_bytetrack_sample.cpp) | YOLOv11 plate + ByteTrack |
| [yolov11_plate_detector_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/yolov11_plate_detector_sample.cpp) | YOLOv11 plate detector |
| [yolov11_plate_detector_trt_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/yolov11_plate_detector_trt_sample.cpp) | YOLOv11 plate TensorRT |

### 2.6 Object Detection & Segmentation Samples

| Sample | Mô tả |
|--------|--------|
| [multi_detectors_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/multi_detectors_sample.cpp) | Nhiều detector song song |
| [multi_detectors_and_classifiers_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/multi_detectors_and_classifiers_sample.cpp) | Detector + Classifier cascade |
| [enet_seg_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/enet_seg_sample.cpp) | Semantic segmentation (ENet) |
| [mask_rcnn_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/mask_rcnn_sample.cpp) | Instance segmentation (Mask R-CNN) |
| [openpose_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/openpose_sample.cpp) | Pose estimation |
| [lane_detect_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/lane_detect_sample.cpp) | Lane detection |
| [obstacle_detect_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/obstacle_detect_sample.cpp) | Obstacle detection |
| [firesmoke_detect_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/firesmoke_detect_sample.cpp) | Fire/smoke detection |
| [video_restoration_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/video_restoration_sample.cpp) | Video super-resolution |
| [yolov11_onnx_detector_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/yolov11_onnx_detector_sample.cpp) | YOLOv11 ONNX detection |
| [yolov11_face_detector_video_output_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/yolov11_face_detector_video_output_sample.cpp) | YOLOv11 face + video output |

### 2.7 TensorRT Samples

| Sample | Mô tả |
|--------|--------|
| [trt_infer_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/trt_infer_sample.cpp) | TensorRT inference cơ bản |
| [trt_yolov8_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/trt_yolov8_sample.cpp) | YOLOv8 TensorRT |
| [trt_yolov8_sample2.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/trt_yolov8_sample2.cpp) | YOLOv8 TensorRT v2 |
| [multi_trt_infer_nodes_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/multi_trt_infer_nodes_sample.cpp) | Nhiều TRT node |
| [yolov11_face_detector_trt_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/yolov11_face_detector_trt_sample.cpp) | YOLOv11-face TRT |
| [yolov11_face_bytetrack_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/yolov11_face_bytetrack_sample.cpp) | YOLOv11-face + ByteTrack |

### 2.8 Rockchip (RKNN) Samples

| Sample | Mô tả |
|--------|--------|
| [rknn_face_detection_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_face_detection_sample.cpp) | Face detection trên RK3588 |
| [rknn_face_detection_file_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_face_detection_file_sample.cpp) | Face detection từ file |
| [rknn_face_detector_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_face_detector_sample.cpp) | Face detector RKNN |
| [rknn_face_tracking_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_face_tracking_sample.cpp) | Face tracking RKNN |
| [rknn_rtsp_tracking_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_rtsp_tracking_sample.cpp) | RTSP tracking RKNN |
| [rknn_rtsp_tracking_mqtt_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_rtsp_tracking_mqtt_sample.cpp) | RTSP + tracking + MQTT (RKNN) |
| [rknn_yolov8_face_detection_simple_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_yolov8_face_detection_simple_sample.cpp) | YOLOv8-face RKNN |
| [rknn_yolov8_face_rtsp_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_yolov8_face_rtsp_sample.cpp) | YOLOv8-face RTSP RKNN |
| [rknn_yolov11_detector_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/rknn_yolov11_detector_sample.cpp) | YOLOv11 RKNN |

### 2.9 Message Broker Samples

| Sample | Mô tả |
|--------|--------|
| [message_broker_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/message_broker_sample.cpp) | JSON console broker (face) |
| [message_broker_sample2.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/message_broker_sample2.cpp) | XML file broker |
| [message_broker_kafka_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/message_broker_kafka_sample.cpp) | Kafka broker |
| [sse_broker_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/sse_broker_sample.cpp) | Server-Sent Events broker |
| [mqtt_json_receiver_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/mqtt_json_receiver_sample.cpp) | MQTT JSON receiver |

### 2.10 Utility & Advanced Samples

| Sample | Mô tả |
|--------|--------|
| [dynamic_pipeline_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/dynamic_pipeline_sample.cpp) | **Hot-plug**: Thêm/xóa node runtime |
| [dynamic_pipeline_sample2.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/dynamic_pipeline_sample2.cpp) | Dynamic pipeline v2 |
| [interaction_with_pipe_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/interaction_with_pipe_sample.cpp) | Tương tác với pipeline |
| [record_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/record_sample.cpp) | Ghi video/ảnh + console interaction |
| [skip_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/skip_sample.cpp) | Bỏ qua frame (giảm FPS) |
| [frame_fusion_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/frame_fusion_sample.cpp) | Ghép 2 frame thành 1 |
| [license_check_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/license_check_sample.cpp) | Kiểm tra license |
| [license_info_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/license_info_sample.cpp) | Đọc thông tin license |
| [cvedix_logger_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/cvedix_logger_sample.cpp) | Demo logger |
| [cvedix_test.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/cvedix_test.cpp) | Test cơ bản |
| [paddle_infer_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/paddle_infer_sample.cpp) | PaddleInference backend |
| [mllm_analyse_sample.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/mllm_analyse_sample.cpp) | Phân tích ảnh bằng LLM (Ollama) |
| [mllm_analyse_sample_openai.cpp](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/mllm_analyse_sample_openai.cpp) | Phân tích ảnh bằng OpenAI API |

---

## 3. Pipeline Pattern phổ biến

### Pattern 1: Basic Detection Pipeline

```
src → primary_detector → osd → des
```

### Pattern 2: Detection + Classification (Cascade)

```
src → primary_detector → secondary_classifier → osd → des
```

### Pattern 3: Detection + Tracking + BA

```
src → detector → tracker → ba_node → ba_osd → des
                                    └→ broker
```

### Pattern 4: Multi-Channel (Resource Sharing)

```
src_0 ─┐                    ┌→ split → des_0
src_1 ─┤→ shared_detector ─→│
src_2 ─┘                    └→ split → des_2
```

### Pattern 5: Dynamic Pipeline (Hot-Plug)

```
// Runtime: Thêm/xóa node bằng attach_to() / detach()
detector->attach_to({new_src});       // thêm source
new_des->attach_to({split});          // thêm output
old_des->detach(); old_des = nullptr; // xóa output
detector->detach_from({"old_src"});   // xóa source
```

### Pattern 6: Fan-out (Parallel Processing)

```
                  ┌→ osd → des (hiển thị)
src → detector ──→├→ broker (gửi dữ liệu)
                  └→ recorder (ghi video)
```

---

## 4. Tài liệu README chi tiết cho từng nhóm

Mỗi nhóm sample có README riêng với hướng dẫn chi tiết:

| README | Nội dung |
|--------|---------|
| [README.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README.md) | Tổng quan tất cả samples |
| [README_BASIC_PIPELINE_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_BASIC_PIPELINE_SAMPLES.md) | Pipeline cơ bản |
| [README_FACE_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_FACE_SAMPLES.md) | Face detect/track/recognize |
| [README_INSIGHTFACE.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_INSIGHTFACE.md) | InsightFace |
| [README_INSIGHTFACE_ONNX.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_INSIGHTFACE_ONNX.md) | InsightFace ONNX |
| [README_INSIGHTFACE_TRT.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_INSIGHTFACE_TRT.md) | InsightFace TensorRT |
| [README_BEHAVIOR_ANALYSIS_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_BEHAVIOR_ANALYSIS_SAMPLES.md) | Behavior analysis |
| [README_DETECTION_SEGMENTATION_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_DETECTION_SEGMENTATION_SAMPLES.md) | Detection & Segmentation |
| [README_VEHICLE_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_VEHICLE_SAMPLES.md) | Vehicle analysis |
| [README_SOURCE_DESTINATION_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_SOURCE_DESTINATION_SAMPLES.md) | Source & Destination |
| [README_MESSAGE_BROKER_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_MESSAGE_BROKER_SAMPLES.md) | Message brokers |
| [README_MQTT_JSON_TRANSFORMER.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_MQTT_JSON_TRANSFORMER.md) | MQTT JSON transformer |
| [README_TENSORRT_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_TENSORRT_SAMPLES.md) | TensorRT samples |
| [README_RKNN_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_RKNN_SAMPLES.md) | RKNN (Rockchip) samples |
| [README_RKNN_YOLOV8_FACE.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_RKNN_YOLOV8_FACE.md) | RKNN YOLOv8-face |
| [README_YOLOV11_ONNX.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_YOLOV11_ONNX.md) | YOLOv11 ONNX |
| [README_YOLOV11_FACE_TRT.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_YOLOV11_FACE_TRT.md) | YOLOv11-face TensorRT |
| [README_UTILITY_SAMPLES.md](file:///home/cvedix/Documents/Github/edgeos-sdk/samples/README_UTILITY_SAMPLES.md) | Utility samples |
