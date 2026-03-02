## Lưu ý quan trọng khi sử dụng node ##
Hầu hết các node hỗ trợ đa kênh (multi-channel), có nghĩa là nhiều kênh dữ liệu có thể được sử dụng làm đầu vào. Ngoài ra, một số node không hỗ trợ đa kênh theo mặc định, tức là chỉ có thể có một kênh dữ liệu làm đầu vào (chỉ số kênh đầu vào không thể thay đổi sau khi đã được xác định), nếu không sẽ xảy ra lỗi. Dưới đây là các ví dụ phổ biến:

*hỗ trợ đa kênh*
- tất cả các node infer, broker node, vì chúng hoạt động độc lập với chỉ số kênh.
- split node, nó cũng hoạt động độc lập với chỉ số kênh. Nó có thể được sử dụng để chia pipeline thành nhiều nhánh theo chỉ số kênh khác nhau.
- một số node đã được thiết kế để hỗ trợ đa kênh, chẳng hạn như record node, track node, ba node, chúng có thể phân biệt các kênh khác nhau trong mã logic (`chẳng hạn như sử dụng std::map<int, ...> để duy trì dữ liệu khác nhau của các kênh`).

*không hỗ trợ đa kênh*
- tất cả src node và tất cả des node, bạn PHẢI chỉ định chỉ số kênh khi khởi tạo instance của chúng.
- một phần của osd node, vì chúng hoạt động phụ thuộc vào chỉ số kênh, bạn có thể thực hiện một số công việc để chúng hỗ trợ đa kênh, ví dụ chỉ cần sử dụng một số cấu trúc dữ liệu như `std::map<int, ...>` để duy trì dữ liệu khác nhau của các kênh trong mã của node.

Lưu ý, mặc dù một số node hỗ trợ đa kênh, bạn nên cẩn thận vì việc sử dụng một instance duy nhất của node để xử lý nhiều kênh dữ liệu sẽ có hiệu suất thấp hơn (xử lý dữ liệu tuần tự). Ngược lại, một instance chỉ xử lý một kênh dữ liệu (nhiều kênh sử dụng nhiều instance của CÙNG node) có hiệu suất cao hơn (xử lý dữ liệu song song). Dưới đây là ví dụ về 2 phương pháp tạo pipeline:
```
một instance của track/ba node hoạt động trên 2 kênh：
file_src_0                                                                                             --> osd_0 --> screen_des_0
           --> detector --> multi-classifiers --> tracker --> ba_crossline --> split(by channel index)
file_src_1                                                                                             --> osd_1 --> screen_des_1 

2 instance của track/ba node hoạt động trên 2 kênh:
file_src_0                                                                --> tracker_0 --> ba_crossline_0 --> osd_0 --> screen_des_0
           --> detector --> multi-classifiers --> split(by channel index) 
file_src_1                                                                --> tracker_0 --> ba_crossline_0 --> osd_1 --> screen_des_1 
```

## Cấu trúc thư mục (Tái cấu trúc năm 2025)

- `common/`：Các lớp cơ sở và hook dùng chung cho tất cả các node（`cvedix_node/src/des_node`、meta publisher/subscriber、`frame_utils` 等）。
  - 📖 [Tài liệu chi tiết](common/README.md)
- `src/`：Tất cả các implementation của source node（`cvedix_file_src_node`、`cvedix_rtsp_src_node`、`cvedix_app_src_node`…）。
  - 📖 [Tài liệu chi tiết](src/README.md)
- `des/`：Tất cả các implementation của destination node（`cvedix_screen_des_node`、`cvedix_file_des_node`、`cvedix_rtmp_des_node`…）。
  - 📖 [Tài liệu chi tiết](des/README.md)
- `mid/`：Các node xử lý trung gian（`split`、`sync`、`message_broker`、`placeholder`、`skip`）。
  - 📖 [Tài liệu chi tiết](mid/README.md)
- `infers/base/`：Các lớp cơ sở cho inference（`cvedix_infer_node`、`primary`、`secondary`），để các model cụ thể trong `nodes/infers/*` tái sử dụng。
  - 📖 [Tài liệu chi tiết](infers/base/README.md)

`nodes/common/frame_utils.h` còn cung cấp các hàm tiện ích như `utils::prepare_output_frame`，được sử dụng để xử lý thống nhất logic phổ biến "OSD/ảnh gốc + tùy chọn resize" trong các des node。


## Danh mục node ##

<details open>
  <summary>ba</summary>

  - cvedix_ba_crossline_node：Phát hiện vượt đường
  - cvedix_ba_jam_node：Phát hiện tắc đường
  - cvedix_ba_stop_node：Phát hiện dừng lại
  - cvedix_ba_area_crowding_node：Phát hiện đám đông trong vùng
  - cvedix_ba_area_dwell_time_node：Đo thời gian lưu lại trong vùng
  - cvedix_ba_area_enter_exit_node：Phát hiện vào/ra vùng
  - cvedix_ba_area_loitering_node：Phát hiện lảng vảng
  - cvedix_ba_area_parking_violation_node：Phát hiện đỗ xe sai quy định
  - cvedix_ba_area_queue_length_node：Đo chiều dài hàng đợi
  - cvedix_ba_fall_detection_node：Phát hiện ngã
  - cvedix_ba_fight_detection_node：Phát hiện đánh nhau
  - cvedix_ba_line_counting_node：Đếm qua đường kẻ
  - cvedix_ba_line_direction_violation_node：Phát hiện đi sai chiều
  - cvedix_ba_line_speed_estimation_node：Ước tính tốc độ
  - cvedix_ba_movement_node：Phát hiện chuyển động
  - **cvedix_ba_line_wrong_way_node**：Phát hiện đi ngược chiều (xác nhận đa đường kẻ)
  - **cvedix_ba_area_lane_violation_node**：Phát hiện vi phạm làn đường theo loại xe
  - **cvedix_ba_line_red_light_violation_node**：Phát hiện vượt đèn đỏ + vượt vạch dừng
  - **cvedix_ba_area_no_entry_zone_node**：Phát hiện vi phạm vùng cấm theo loại xe/giờ
  - **cvedix_ba_area_illegal_turn_node**：Phát hiện rẽ không đúng quy định tại ngã tư
  - **cvedix_ba_area_helmet_violation_node**：Phát hiện không đội mũ bảo hiểm
  - **cvedix_ba_line_illegal_uturn_node**：Phát hiện quay đầu xe sai quy định
</details>

<details open>
  <summary>broker</summary>
  
  - cvedix_ba_socket_broker_node：Chuyển tiếp kết quả phân tích hành vi bằng udp
  - cvedix_embeddings_properties_socket_broker_node：Chuyển tiếp kết quả đặc trưng và thuộc tính đối tượng bằng udp
  - cvedix_embeddings_socket_broker_node：Chuyển tiếp kết quả đặc trưng đối tượng bằng udp
  - cvedix_expr_socket_broker_node：Chuyển tiếp kết quả kiểm tra biểu thức toán học bằng udp
  - cvedix_json_console_broker_node：Xuất dữ liệu có cấu trúc ra console ở định dạng json
  - cvedix_json_kafka_broker_node：Gửi dữ liệu có cấu trúc đến bên thứ ba qua kafka ở định dạng json
  - cvedix_msg_broker_node：Node lớp cơ sở cho data broker
  - cvedix_plate_socket_broker_node：Chuyển tiếp kết quả nhận dạng biển số bằng udp
  - cvedix_xml_file_broker_node：Lưu trữ dữ liệu có cấu trúc vào file ở định dạng xml
  - cvedix_xml_socket_broker_node：Gửi dữ liệu có cấu trúc đến bên thứ ba qua udp ở định dạng xml
</details>

<details open>
  <summary>infers</summary>

  - cvedix_yolo_detector_node：Phát hiện vật thể YOLO (ONNX/DNN)
  - cvedix_yolov11_detector_node：Phát hiện vật thể YOLOv11 (ONNX)
  - cvedix_trt_yolov11_detector_node：Phát hiện vật thể YOLOv11 (TensorRT)
  - cvedix_classifier_node：Phân loại ảnh (secondary)
  - cvedix_feature_encoder_node：Trích xuất embedding (ReID)
  - cvedix_facenet_node：Nhận diện khuôn mặt
  - cvedix_mllm_analyser_node：Phân tích ảnh bằng LLM
  - **cvedix_clip_node**：CLIP zero-shot classification (full-frame → description)
  - **cvedix_clip_secondary_node**：CLIP per-target classification (secondary → labels + embeddings)
</details>
  
  - cvedix_classifier_node：Node phân loại hình ảnh dựa trên resnet series（opencv::dnn）
  - cvedix_enet_seg_node：Node phân đoạn hình ảnh dựa trên mạng ENet（opencv::dnn）
  - cvedix_face_swap_node：Node thay thế khuôn mặt dựa trên insightface（opencv::dnn）
  - cvedix_feature_encoder_node：Node trích xuất đặc trưng đối tượng dựa trên resnet series（opencv::dnn）
  - cvedix_lane_detector_node：Node phát hiện làn đường dựa trên CenterNet（opencv::dnn）
  - cvedix_mask_rcnn_detector_node：Node phát hiện đối tượng dựa trên maskrcnn（opencv::dnn）
  - cvedix_openpose_detector_node：Node phát hiện tư thế cơ thể dựa trên openpose（opencv::dnn）
  - cvedix_ppocr_text_detector_node：Node phát hiện văn bản dựa trên paddleocr（paddleinference）
  - cvedix_restoration_node：Node nâng cấp và khôi phục hình ảnh dựa trên real-esrgan（opencv::dnn）
  - cvedix_sface_feature_encoder_node：Node trích xuất đặc trưng khuôn mặt dựa trên mạng sface（opencv::dnn）
  - cvedix_trt_insight_face_recognition_node：Node trích xuất đặc trưng khuôn mặt dựa trên InsightFace ArcFace（tensorrt）
  - ~~cvedix_trt_vehicle_color_classifier~~ (đã xóa - TRT 10.x incompatible)
  - ~~cvedix_trt_vehicle_detector~~ (đã xóa - thay bằng cvedix_trt_yolov11_detector_node)
  - ~~cvedix_trt_vehicle_feature_encoder~~ (đã xóa - TRT 10.x incompatible)
  - ~~cvedix_trt_vehicle_plate_detector_v2~~ (đã xóa - thay bằng cvedix_trt_yolov11_plate_detector_node)
  - ~~cvedix_trt_vehicle_plate_detector~~ (đã xóa - thay bằng cvedix_trt_yolov11_plate_detector_node)
  - ~~cvedix_trt_vehicle_scanner~~ (đã xóa - TRT 10.x incompatible)
  - ~~cvedix_trt_vehicle_type_classifier~~ (đã xóa - TRT 10.x incompatible)
  - cvedix_yolo_detector_node：Node phát hiện đối tượng dựa trên yolov3（bao gồm tiny）（opencv::dnn）
  - yolo_yunet_face_detector_node：Node phát hiện khuôn mặt dựa trên mạng yunet（opencv::dnn）

</details>

<details open>
  <summary>osd</summary>
  
  - cvedix_ba_crossline_osd_node：Node vẽ kết quả phát hiện vượt đường
  - cvedix_ba_jam_osd_node：Node vẽ kết quả phát hiện tắc đường
  - cvedix_ba_stop_osd_node：Node vẽ kết quả phát hiện dừng lại
  - cvedix_cluster_node：Node vẽ kết quả phân cụm đối tượng
  - cvedix_expr_osd_node：Node vẽ kết quả kiểm tra biểu thức toán học
  - cvedix_face_osd_node_v2：Node vẽ kết quả phát hiện khuôn mặt（bao gồm hiển thị độ tương đồng）
  - cvedix_face_osd_node：Node vẽ kết quả phát hiện khuôn mặt
  - cvedix_lane_osd_node：Node vẽ kết quả phát hiện làn đường
  - cvedix_osd_node_v2：Node vẽ đối tượng（bao gồm đối tượng con）
  - cvedix_osd_node_v3：Node vẽ đối tượng（bao gồm mask đối tượng）
  - cvedix_osd_node：Node vẽ đối tượng
  - cvedix_plate_osd_node：Node vẽ kết quả phát hiện và nhận dạng biển số
  - cvedix_pose_osd_node：Node vẽ kết quả phát hiện tư thế cơ thể
  - cvedix_seg_osd_node：Node vẽ kết quả phân đoạn hình ảnh
  - cvedix_text_osd_node：Node vẽ kết quả phát hiện và nhận dạng văn bản
</details>

<details open>
  <summary>proc</summary>
  
  - cvedix_expr_check_node：Node kiểm tra độ chính xác của phương trình toán học
  - cvedix_frame_fusion_node：Node hợp nhất khung hình video theo tỷ lệ pixel（hỗ trợ 2 kênh）
</details>

<details open>
  <summary>record</summary>
  
  - cvedix_record_node：Node ghi video/hình ảnh
</details>

<details open>
  <summary>track</summary>
  
  - cvedix_dsort_track_node：Node theo dõi dựa trên deepsort
  - cvedix_sort_track_node：Node theo dõi dựa trên sort
</details>

<details open>
  <summary>common</summary>
  
  - cvedix_app_des_node：Node đích đẩy dữ liệu hình ảnh đến application
  - cvedix_app_src_node：Node nguồn nhận dữ liệu hình ảnh từ application
  - cvedix_des_node：Lớp cơ sở cho tất cả các node đích
  - cvedix_fake_des_node：Node đích ảo（không làm gì cả）
  - cvedix_file_des_node：Node đích lưu dữ liệu video vào file
  - cvedix_file_src_node：Node nguồn đọc dữ liệu video từ file
  - cvedix_image_des_node：Node đích gửi dữ liệu dưới dạng hình ảnh đến socket hoặc file
  - cvedix_image_src_node：Node nguồn đọc dữ liệu hình ảnh từ file hoặc socket
  - cvedix_infer_node：Lớp cơ sở cho tất cả các node inference
  - cvedix_message_broker_node：Lớp cơ sở cho tất cả các node data broker
  - cvedix_node：Lớp cơ sở cho tất cả các node
  - cvedix_placeholder_node：Node trung gian ảo（không làm gì cả）
  - cvedix_primary_infer_node：Lớp cơ sở cho tất cả các node inference cấp một
  - cvedix_rtmp_des_node：Node đích đẩy dữ liệu video đến rtmp server ở định dạng rtmp
  - cvedix_rtsp_des_node：Node đích đẩy dữ liệu video ở định dạng rtsp（không cần rtsp server）
  - cvedix_rtsp_src_node：Node nguồn đọc stream mạng ở định dạng rtsp
  - cvedix_screen_des_node：Node đích hiển thị video/hình ảnh lên màn hình
  - cvedix_secondary_infer_node：Lớp cơ sở cho tất cả các node inference cấp hai
  - cvedix_split_node：Node chia tách pipeline
  - cvedix_src_node：Lớp cơ sở cho tất cả các node nguồn
  - cvedix_sync_node：Node đồng bộ các nhánh pipeline
  - cvedix_udp_src_node：Node nguồn đọc stream mạng ở định dạng udp
</details>
