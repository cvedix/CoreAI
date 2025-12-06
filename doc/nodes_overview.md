# Tài liệu node (nodes/README.md)

Tài liệu này tổng hợp nội dung chính từ `nodes/README.md`, bổ sung mô tả đầu vào (input) thực tế của từng node dựa trên mã nguồn hiện hành. Nội dung được viết bằng tiếng Việt để thuận tiện cho đội phát triển, đồng thời nhắc lại các khái niệm meta cốt lõi giúp xây dựng pipeline chính xác.

## 1. Lưu ý đa kênh

- Đa số middle node và infer node hỗ trợ đa kênh: một instance có thể nhận nhiều luồng (`channel_index`) khác nhau, tự tách trạng thái bằng các cấu trúc như `std::map<int, …>`.
- Source node (`src`) và destination node (`des`) **không** hỗ trợ đa kênh mặc định: phải tạo instance riêng cho từng `channel_index`.
- Một instance xử lý nhiều kênh sẽ tuần tự hóa công việc → đơn giản nhưng chậm hơn. Với yêu cầu hiệu năng cao, nên tạo nhiều instance chạy song song, mỗi instance đảm nhiệm một kênh.

## 2. Cấu trúc thư mục node (tái cấu trúc 2025)

- `nodes/common/`: lớp nền tảng dùng chung (`cvedix_node`, `cvedix_src_node`, `frame_utils`…).
- `nodes/src/`: các nguồn dữ liệu (file, RTSP, ứng dụng…).
- `nodes/des/`: các đích xuất dữ liệu (file, RTMP, màn hình…).
- `nodes/mid/`: node trung gian (split, sync, message broker cơ sở…).
- `nodes/infers/`: lớp và node suy luận (primary/secondary, TensorRT, RKNN…).
- `nodes/osd/`: node hiển thị overlay.
- `nodes/ba/`: node phân tích hành vi (Behaviour Analysis).
- `nodes/proc/`: node xử lý hậu kỳ (kiểm tra biểu thức, ghép khung hình…).
- `nodes/record/`: node ghi hình/ảnh.
- `nodes/track/`: node tracking (SORT, Deep SORT).

## 3. Kiến thức nền về metadata

- `cvedix_frame_meta`: gói dữ liệu chính của mỗi khung hình.
  - Chứa `frame`, `osd_frame`, `mask`, `frame_index`, `channel_index`, `fps`.
  - Danh sách `targets` (đối tượng thường), `pose_targets`, `face_targets`, `text_targets`.
  - `ba_results`: kết quả hành vi (do BA node ghi).
- `cvedix_frame_target`: đối tượng phát hiện/track gồm vị trí, `primary_class_id`, nhãn phụ (`secondary_labels`), embedding, lịch sử `tracks`.
- `cvedix_frame_face_target` / `cvedix_frame_text_target` / `cvedix_frame_pose_target`: các biến thể chuyên biệt cho khuôn mặt, văn bản, pose.
- `cvedix_control_meta`: siêu dữ liệu điều khiển (ví dụ lệnh ghi hình) mà một số node vừa tạo vừa tiêu thụ.

Khi mô tả “Input” bên dưới, mặc định node nhận `std::shared_ptr<cvedix_frame_meta>` (và đôi khi cả `cvedix_control_meta`) từ upstream nếu không ghi chú khác.

## 4. Danh mục node & đầu vào chính

### 4.1. Nhóm Behaviour Analysis (BA)

- **cvedix_ba_crossline_node**
  - Mô tả: phát hiện đối tượng cắt qua một đường (line) định nghĩa trước.
  - Input: `cvedix_frame_meta` có `targets` đã được tracker gán `track_id` & `tracks` ≥ 2 điểm. Cần cấu hình line cho từng kênh.
- **cvedix_ba_jam_node**
  - Mô tả: phát hiện ùn tắc trong một vùng polygon.
  - Input: `frame_meta` có `targets` kèm lịch sử `tracks`, `fps` chính xác. Cần cấu hình vùng jam per channel; yêu cầu tracker upstream để theo dõi chuyển động.
- **cvedix_ba_stop_node**
  - Mô tả: phát hiện đối tượng dừng trong vùng giám sát.
  - Input: tương tự `ba_jam_node` nhưng ngưỡng kiểm tra khoảng cách nhỏ hơn; cần tracker upstream và cấu hình polygon.

### 4.2. Nhóm Broker

- **cvedix_msg_broker_node** (lớp cơ sở)
  - Mô tả: nhận `frame_meta`, serialize dữ liệu rồi đẩy bất đồng bộ tới thread broking riêng.
  - Input: `cvedix_frame_meta` (và `cvedix_control_meta`); dùng chung cho các broker con. Tham số `cvedix_broke_for` quyết định làm việc với `targets`, `face_targets`, `text_targets`, `pose_targets`.
- **cvedix_json_console_broker_node**
  - Mô tả: ghi JSON của meta ra console (debug).
  - Input: `frame_meta`; hỗ trợ mọi loại target tùy giá trị `broke_for`.
- **cvedix_json_kafka_broker_node**
  - Mô tả: phát JSON lên Kafka.
  - Input: `frame_meta`; cần cấu hình broker Kafka, topic.
- **cvedix_xml_file_broker_node**
  - Mô tả: ghi meta thành XML vào file.
  - Input: `frame_meta`; đường dẫn file cần hợp lệ, node tự mở/ghi tuần tự.
- **cvedix_xml_socket_broker_node**
  - Mô tả: gửi XML qua UDP.
  - Input: `frame_meta`; cần cấu hình IP/port; dữ liệu gửi theo định dạng XML đã serialize.
- **cvedix_ba_socket_broker_node**
  - Mô tả: gửi kết quả BA ra UDP.
  - Input: `frame_meta` có `ba_results` do các BA node ghi vào; giả định `targets` chứa `track_id` để ánh xạ.
- **cvedix_expr_socket_broker_node**
  - Mô tả: gửi kết quả kiểm tra biểu thức toán (TEXT) ra UDP, kèm ảnh screenshot.
  - Input: `frame_meta` với `text_targets` đã có cờ `yes/no/invalid`, `osd_frame` hoặc `frame` để lưu ảnh; `broke_for` phải là `TEXT`.
- **cvedix_embeddings_socket_broker_node**
  - Mô tả: xuất embedding (đối tượng thường hoặc khuôn mặt) kèm ảnh crop qua UDP.
  - Input: `frame_meta`:
    - Với `broke_for::NORMAL`: cần `targets` có `embeddings` (do secondary encoder tạo), optional `track_id`, kích thước tối thiểu.
    - Với `broke_for::FACE`: sử dụng `face_targets` (cũng cần embedding).
- **cvedix_embeddings_properties_socket_broker_node**
  - Mô tả: gửi embedding + thuộc tính phụ (secondary labels) và sub-target (ví dụ biển số) qua UDP.
  - Input: `frame_meta` có `targets` chứa `embeddings`, `secondary_labels`, `sub_targets`. Tùy chọn chỉ gửi khi đã track ổn định.
- **cvedix_plate_socket_broker_node**
  - Mô tả: gửi thông tin biển số (màu + text) và ảnh crop qua UDP.
  - Input: `frame_meta` có `targets` mà `primary_label` chứa chuỗi `color_text`; yêu cầu track ID nếu bật tránh trùng; cần ảnh gốc để cắt biển số.

### 4.3. Nhóm Inference

**Primary inference (làm việc trên toàn khung hình):**

- **cvedix_yolo_detector_node**
  - Mô tả: phát hiện đối tượng YOLOv3/v4/v5 (OpenCV DNN).
  - Input: `frame_meta->frame`; trả kết quả vào `targets` (bbox, score, label) với `class_id_offset`.
- **cvedix_mask_rcnn_detector_node**
  - Mô tả: Mask R-CNN phát hiện + mask đối tượng.
  - Input: `frame_meta->frame`; cập nhật `targets` và `target->mask`.
- **cvedix_enet_seg_node**
  - Mô tả: phân đoạn ENet.
  - Input: `frame_meta->frame`; ghi mask toàn cảnh vào `frame_meta->mask`.
- **cvedix_lane_detector_node**
  - Mô tả: phát hiện làn đường (CenterNet).
  - Input: `frame_meta->frame`; ghi mask lane vào `frame_meta->mask`.
- **cvedix_openpose_detector_node**
  - Mô tả: phát hiện pose cơ thể (OpenPose).
  - Input: `frame_meta->frame`; tạo `pose_targets` với danh sách keypoint, skeleton.
- **cvedix_ppocr_text_detector_node**
  - Mô tả: OCR PaddleOCR (det + rec).
  - Input: `frame_meta->frame`; sinh `text_targets` (polygon + text + score).
- **cvedix_restoration_node**
  - Mô tả: khôi phục/nâng cấp ảnh (Real-ESRGAN).
  - Input: `frame_meta->frame`; ghi kết quả vào `osd_frame` (nếu `restoration_to_osd`) hoặc thay thế `frame`.
- **cvedix_face_swap_node**
  - Mô tả: thay khuôn mặt bằng mẫu cho trước (InsightFace).
  - Input: ưu tiên `frame_meta->face_targets` có keypoints; nếu `act_as_primary_detector=true` sẽ tự phát hiện. Sử dụng `frame`/`osd_frame` để dán kết quả.
- **cvedix_trt_vehicle_detector**, **cvedix_trt_vehicle_plate_detector_v2**, **cvedix_trt_vehicle_scanner**, **cvedix_trt_yolov8_detector**, **cvedix_trt_yolov8_pose_detector**, **cvedix_trt_yolov8_seg_detector**, **cvedix_yunet_face_detector_node**, **cvedix_rknn_yolov8_detector_node**
  - Mô tả: họ node TensorRT/RKNN/ONNX phát hiện đối tượng, biển số, cơ thể, mặt…
  - Input: `frame_meta->frame`; trả kết quả vào `targets` hoặc `face_targets` tùy loại. Một số node yêu cầu GPU/NPU tương ứng, cần model path hợp lệ.

**Secondary inference (làm việc trên crop đối tượng):**

- **cvedix_classifier_node**
  - Mô tả: phân loại thứ cấp (ResNet).
  - Input: `frame_meta->targets`; chỉ xử lý các target có `primary_class_id` nằm trong `p_class_ids_applied_to`, đủ kích thước. Ghi nhãn vào `secondary_class_ids/labels/scores`.
- **cvedix_feature_encoder_node**
  - Mô tả: trích xuất embedding chung (ResNet).
  - Input: `frame_meta->targets`; yêu cầu bounding box trước đó. Ghi embedding vào `target->embeddings`.
- **cvedix_sface_feature_encoder_node**
  - Mô tả: embedding khuôn mặt (SFace).
  - Input: `frame_meta->face_targets` với keypoints; node tự căn chỉnh rồi ghi embedding.
- **cvedix_trt_vehicle_color_classifier**, **cvedix_trt_vehicle_type_classifier**
  - Mô tả: phân loại màu/loại xe (TensorRT).
  - Input: `frame_meta->targets`; lọc theo class ID & kích thước; cập nhật `secondary_labels/scores`.
- **cvedix_trt_vehicle_feature_encoder**
  - Mô tả: trích xuất embedding xe (TensorRT).
  - Input: `frame_meta->targets`; yêu cầu bounding box xe; kết quả ghi vào `target->embeddings`.
- **cvedix_trt_vehicle_plate_detector**
  - Mô tả: phát hiện + nhận dạng biển số trên crop xe (hai giai đoạn).
  - Input: `frame_meta->targets` đại diện xe; node cắt ảnh con, tìm sub-target (biển số) và gán text/score vào `sub_targets` & `secondary_labels`.
- **cvedix_trt_vehicle_color_classifier**, **cvedix_trt_vehicle_type_classifier**, **cvedix_trt_yolov8_classifier**
  - Input chung: `targets` đã phát hiện, thường yêu cầu `track_id` nếu cấu hình chỉ gửi đối tượng đã ổn định.

### 4.4. Nhóm OSD

- **cvedix_osd_node** / **cvedix_osd_node_v2/v3**
  - Mô tả: vẽ khung, nhãn, sub-target (V2 có hỗ trợ cấu trúc con, V3 hỗ trợ mask).
  - Input: `frame_meta->targets`, `sub_targets`, `mask`; tạo/ghi `osd_frame`. Cần font nếu muốn vẽ Unicode.
- **cvedix_face_osd_node** / **cvedix_face_osd_node_v2**
  - Mô tả: hiển thị kết quả nhận dạng khuôn mặt, độ tương đồng.
  - Input: `face_targets` (bbox, embedding, label). Phiên bản v2 hiển thị thêm thông tin so khớp.
- **cvedix_plate_osd_node**
  - Mô tả: vẽ biển số, màu biển, lịch sử ở cuối khung hình.
  - Input: `targets` chứa thông tin biển số trong `primary_label` (màu_text), optional `track_id`.
- **cvedix_ba_crossline_osd_node / cvedix_ba_jam_osd_node / cvedix_ba_stop_osd_node**
  - Mô tả: hiển thị kết quả BA tương ứng.
  - Input: `targets` (để vẽ track/hộp) + `ba_results` để hiển thị line/region và số liệu.
- **cvedix_seg_osd_node**
  - Mô tả: overlay mask phân đoạn.
  - Input: `frame_meta->mask` hoặc mask từng target; sử dụng `osd_frame`.
- **cvedix_pose_osd_node**
  - Mô tả: vẽ skeleton từ `pose_targets`.
  - Input: `pose_targets` (keypoints).
- **cvedix_lane_osd_node**
  - Mô tả: vẽ lane/mask làn đường.
  - Input: mask lane hoặc các điểm lane do node lane detector cung cấp.
- **cvedix_text_osd_node**
  - Mô tả: hiển thị text detection/OCR và nhãn xác thực.
  - Input: `text_targets` với polygon, text, cờ `yes/no`.
- **cvedix_cluster_node**
  - Mô tả: hiển thị kết quả gom cụm embedding.
  - Input: `targets` có `cluster_id`/embedding.
- **cvedix_expr_osd_node**
  - Mô tả: vẽ kết quả kiểm tra biểu thức (đúng/sai/invalid).
  - Input: `text_targets` với flag `yes/no/invalid`.

### 4.5. Nhóm Proc

- **cvedix_expr_check_node**
  - Mô tả: đánh giá biểu thức toán dạng “a=b”.
  - Input: `frame_meta->text_targets` (chuỗi OCR). Node đặt cờ `yes/no/invalid` trong `flags`.
- **cvedix_frame_fusion_node**
  - Mô tả: ghép 2 kênh thành 1 bằng ma trận biến đổi hình học.
  - Input: `frame_meta` từ nhiều kênh; cần cấu hình 4 điểm chuẩn của kênh nguồn & đích. Node giữ cache `frame_meta` tạm rồi ghép vào `osd_frame` đích.

### 4.6. Nhóm Record

- **cvedix_record_node**
  - Mô tả: ghi video/ảnh bất đồng bộ, hỗ trợ ghi trước (pre-record) và lệnh điều khiển.
  - Input: 
    - `frame_meta` để lấy frame (gốc hoặc `osd_frame`) cho từng kênh.
    - `cvedix_image_record_control_meta` / `cvedix_video_record_control_meta` làm tín hiệu bắt đầu/dừng ghi, do node khác (ví dụ BA) phát.

### 4.7. Nhóm Track

- **cvedix_track_node** (cơ sở)
  - Mô tả: quản lý pipeline tracking chung (cache `tracks`, cập nhật `track_id`).
  - Input: `frame_meta` có `targets` hoặc `face_targets`; yêu cầu bounding box từ detector và, nếu có, embedding để tăng độ chính xác.
- **cvedix_sort_track_node**
  - Mô tả: tracking theo thuật toán SORT (Kalman + Hungarian).
  - Input: `targets` (bbox); embedding không bắt buộc.
- **cvedix_dsort_track_node**
  - Mô tả: tracking Deep SORT (dùng embedding).
  - Input: `targets` có `embeddings`; nếu thiếu embedding node vẫn chạy nhưng độ chính xác giảm.

### 4.8. Nhóm Middle khác (split/sync/placeholder/skip)

- **cvedix_split_node**
  - Mô tả: tách pipeline nhiều nhánh với tùy chọn deep copy hoặc lọc theo `channel_index`.
  - Input: bất kỳ `cvedix_meta`; nếu `split_with_deep_copy=true` cần cân nhắc chi phí clone.
- **cvedix_sync_node**
  - Mô tả: đồng bộ meta từ nhiều nhánh song song theo `frame_index`.
  - Input: `frame_meta` (và `control_meta`); nên dùng cùng `channel_index`. Chế độ `MERGE` gộp danh sách target, `UPDATE` cập nhật thuộc tính target.
- **cvedix_placeholder_node**
  - Mô tả: node rỗng làm điểm neo pipeline.
  - Input: passthrough `cvedix_meta`, không thay đổi.
- **cvedix_skip_node**
  - Mô tả: bỏ qua có điều kiện (ví dụ giảm FPS).
  - Input: `cvedix_meta`; logic skip dựa trên cấu hình nội bộ.

### 4.9. Nhóm Source (`src/`) – nhắc nhanh

- **cvedix_file_src_node**, **cvedix_rtsp_src_node**, **cvedix_rtmp_src_node**, **cvedix_udp_src_node**
  - Input: nguồn dữ liệu bên ngoài (đường dẫn file, URL, stream). Xuất `frame_meta` với `frame`, `fps`, `channel_index`.
- **cvedix_image_src_node**
  - Input: thư mục ảnh hoặc socket hình ảnh.
- **cvedix_app_src_node**
  - Input: buffer chia sẻ từ ứng dụng bên ngoài.

### 4.10. Nhóm Destination (`des/`)

- **cvedix_screen_des_node**
  - Mô tả: hiển thị lên màn hình (GStreamer).
  - Input: `frame_meta` (ưu tiên `osd_frame`, fallback `frame`); cần cấu hình sink theo môi trường (`DISPLAY`, `WAYLAND_DISPLAY`, `kmssink`…).
- **cvedix_file_des_node**
  - Mô tả: ghi video ra file.
  - Input: `frame_meta`; dùng `utils::prepare_output_frame` để chọn `osd_frame` hoặc `frame`. Hỗ trợ điều khiển bitrate, phân đoạn.
- **cvedix_image_des_node**
  - Mô tả: xuất ảnh tĩnh qua file/socket.
  - Input: `frame_meta`; tùy chọn ghi liên tục (ảnh chụp khung).
- **cvedix_rtmp_des_node**, **cvedix_rtsp_des_node**
  - Input: `frame_meta` (khung hình đã render). `rtsp_des` tự chạy RTSP server (yêu cầu `gstreamer-rtsp-server`).
- **cvedix_app_des_node**
  - Input: `frame_meta`; đẩy frame tới ứng dụng qua shared memory/IPC.
- **cvedix_fake_des_node**
  - Input: `frame_meta`; bỏ qua (testing).

### 4.11. Lớp nền tảng chung

- **cvedix_node**
  - Input: nhận `cvedix_meta` từ upstream thông qua `meta_flow`. Cung cấp thread xử lý, queue in/out, hook, publisher/subscriber.
- **cvedix_src_node / cvedix_des_node / cvedix_mid (gián tiếp)**
  - Quy định hành vi cụ thể của nguồn/đích. Source override `handle_run` tự tạo `frame_meta`; destination override `handle_frame_meta` và bỏ qua `dispatch_run`.
- **cvedix_infer_node**, **cvedix_primary_infer_node**, **cvedix_secondary_infer_node**
  - Định nghĩa pipeline chuẩn (prepare → preprocess → infer → postprocess).
  - Input: `frame_meta`; secondary cần `targets`/`face_targets`.
- **cvedix_message_broker_node**
  - Input: `frame_meta` + `control_meta`; phục vụ serialize và truyền thông ra ngoài.

## 5. Ghi chú bổ sung

- Repo hiện có thêm một số node TensorRT/YOLOv8, RKNN… chưa được liệt kê trong `nodes/README.md`. Tài liệu này đã nhắc tới các node chính liên quan tới README; khi đưa node mới vào PDF cần cập nhật phần mô tả & đầu vào tương ứng.
- Khi xây dựng pipeline, luôn đảm bảo luồng meta phù hợp: detector → (track) → secondary infer → BA → OSD/Broker. Thiếu bước trung gian (ví dụ không có tracker nhưng dùng BA) sẽ khiến node không nhận đủ thông tin trong `frame_meta`.


