Các node trong nhóm `proc` là các bước xử lý trung gian, không phải nguồn (`src`) hay đích (`des`). Hiện thư mục này có ba node:

---

### 1. `cvedix_expr_check_node`
- **Chức năng:** kiểm tra tính đúng sai của biểu thức toán dạng `vế trái = vế phải`. Node duyệt qua từng `text_target` trong frame (thường do OSD/nhận dạng chữ tạo ra).
- **Cách hoạt động:**
  1. Tách chuỗi dựa vào dấu `=` → phải có đúng 2 phần, nếu thiếu thì đánh dấu `invalid`.
  2. Chuẩn hoá ký hiệu (đổi `x` thành `*`, `÷` thành `/`, …), kiểm tra rỗng.
  3. Parse vế phải sang số; nếu lỗi thì `invalid`.
  4. Dùng TinyExpr (`te_interp`) để tính giá trị vế trái. Nếu parser báo lỗi → `invalid`.
  5. So sánh kết quả:  
     - Bằng nhau → `flags = "yes_<giá trị>"`  
     - Sai → `flags = "no_<giá trị>"`.
- **Trả về:** frame meta ban đầu, nhưng `text_targets[i]->flags` đã được gán “yes/no/invalid”, giúp các node downstream hiển thị hoặc cảnh báo.

---

### 2. `cvedix_frame_fusion_node`
- **Chức năng:** ghép (fuse) hai video frame từ hai kênh khác nhau dựa trên 4 điểm hiệu chỉnh (homography). Dùng để tạo ảnh tổng hợp (ví dụ ghép camera toàn cảnh).
- **Thông số khởi tạo:**
  - `src_points`, `des_points`: 4 cặp điểm tương ứng trong ảnh nguồn và ảnh đích.
  - `src_channel_index`, `des_channel_index`: kênh nguồn và kênh đích (khác nhau).
- **Cách hoạt động:**
  1. Tính ma trận biến đổi phối cảnh `trans_mat = getPerspectiveTransform()`.
  2. Node giữ tạm frame của kênh đích (`tmp_des`). Khi nhận frame kênh nguồn:
     - Warp nguồn sang perspective đích (`warpPerspective`),
     - Pha trộn với ảnh đích (0.7/0.3) để tạo kết quả vào `osd_frame`.
  3. `tmp_des` được đẩy downstream trước, sau đó frame nguồn được trả về (không giữ lại).
  4. Nếu chưa có frame đích tương ứng, frame nguồn sẽ được bỏ qua lần đó (đợi vòng sau).
- **Lưu ý:** không đồng bộ theo timestamp; chỉ fuse “lần lượt” khi cả hai kênh đã có frame.

---

### 3. `cvedix_motion_detection_node`
- **Chức năng:** phát hiện vùng chuyển động bằng OpenCV MOG2 trước các node inference.
- **Đầu ra:** mask nhị phân toàn frame (`CV_8UC1`) trong `frame_meta->mask`.
- **Cấu hình chính:**
  - `history`, `var_threshold`: lịch sử và độ nhạy của background model.
  - `min_area_ratio`: bỏ các vùng chuyển động quá nhỏ.
  - `warmup_frames`, `hold_frames`: ổn định model nền và giữ trạng thái motion.
  - `processing_width`: giảm kích thước xử lý để tiết kiệm CPU.
  - `drop_static_frames`: nếu bật, frame tĩnh không được chuyển downstream.
- **Mặc định:** giữ luồng video liên tục (`drop_static_frames = false`) và chỉ bổ sung motion mask cho các node downstream.
- **Runtime:** hỗ trợ `update_config()` cùng các metrics `frames_seen()`, `motion_frames()`, `last_region_count()` và `motion_active()`.

---

Tóm lại, `proc` là nhóm “processing nodes”:
- `cvedix_expr_check_node`: kiểm tra biểu thức toán trên dữ liệu text trong frame.
- `cvedix_frame_fusion_node`: ghép hai khung hình theo ma trận hiệu chỉnh để tạo view tổng hợp.
- `cvedix_motion_detection_node`: tạo foreground motion mask bằng MOG2.
