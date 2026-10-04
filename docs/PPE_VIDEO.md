# Phát triển luồng nhận diện PPE từ video

Luồng C++ mặc định suy luận trên NVIDIA GPU bằng engine TensorRT FP16, dùng các node CoreAI:

```text
OpenCV VideoCapture -> app_src -> yolo_detector (YOLO11 / TensorRT FP16)
                    -> osd -> app_des -> MP4 + CSV
```

Video trong workspace nằm tại `data/videos/16.22.09.mp4`, không phải
`data/models/videos/16.22.09.mp4`. Model mặc định là
`data/models/yolov11n_ppe_detection_fp16.engine`, được tối ưu từ
`data/models/yolov11n_ppe_detection.onnx`. `--backend onnx` chọn model ONNX
mặc định và chạy CPU. Backend được chọn rõ ràng, không tự chuyển CPU nếu GPU lỗi.

Metadata của model xác nhận input float32 `[1,3,640,640]`, output
`[1,6,8400]`, class `0 = mũ`, `1 = Áo`. File `configs/ppe_labels.txt`
dùng `Mu`, `Ao` để OSD mặc định hiển thị được bằng font OpenCV.
Model PPE chỉ phát hiện mũ/áo. Chế độ hai nhánh bên dưới bổ sung model người
để đánh giá đủ/thiếu PPE trên từng người.

## Demo người + mũ + áo chạy song song

```text
                       +-> PPE detector (TensorRT, Mu=0 / Ao=1) ----+
Video -> app_src -> split                                         +-> merge -> ByteTrack -> PPE safety -> MP4/CSV
                       +-> person detector (TensorRT, person=100) -+
```

```bash
# Sau khi thêm node mới, cấu hình lại để CMake thu thập source:
cmake -S . -B build
bash scripts/run_ppe_safety_video.sh

# Kiểm tra nhanh / điều chỉnh ngưỡng người:
bash scripts/run_ppe_safety_video.sh --max-frames 30 --person-conf 0.35
```

Script dùng `data/models/yolov11-cetection_fp16.engine` cho nhánh người và
`data/models/yolov11n_ppe_detection_fp16.engine` cho nhánh PPE. Tên output
mặc định có thời gian chạy, tránh ghi đè lần trước. Có thể dùng trực tiếp:

```bash
bash scripts/run_ppe_video.sh \
  --person-model data/models/yolov11-cetection_fp16.engine \
  --tracking bytetrack --analysis-board 1 \
  --person-conf 0.35 --conf 0.25 --nms 0.45 \
  --output output/ppe/safety_custom.mp4
```

- Model người có input `image [1,3,640,640]`, ba output float32
  `boxes [1,8400,4]` (xyxy), `scores [1,8400]`, `class_idx [1,8400]`.
  Backend TensorRT giải mã trực tiếp, hoàn tác letterbox và NMS theo lớp.
  Giữ class người gốc `0`, đổi thành `100` để tách khỏi class mũ `0`.
- `split` sao chép metadata/ảnh từ frame gốc riêng cho mỗi detector, tránh
  sao chép kết quả đã bị nhánh chạy trước thay đổi. Mỗi detector có luồng
  xử lý và CUDA stream riêng. `merge` đồng bộ cùng channel/frame; sample chỉ
  gửi frame tiếp theo khi nhận kết quả đầy đủ của frame hiện tại. Nếu nhánh
  không trả về, sample báo timeout sau 60 giây.
- Node `cvedix_ba_ppe_safety_node` ghép mũ ở vùng đầu và áo ở vùng thân.
  Ít nhất 60% diện tích món PPE phải nằm trong hộp người mở rộng nhẹ; tâm mũ
  nằm trong -10% đến 45% chiều cao người, tâm áo nằm trong 15% đến 80%.
  Chọn cặp gần vị trí đầu/thân nhất trước; mỗi món và mỗi vị trí PPE trên một
  người được ghép tối đa một lần trong frame.
- ByteTrack nằm sau `merge`, chỉ track người (class 100), giữ nguyên các phát
  hiện mũ/vest để ghép PPE. Khung người hiển thị `Nguoi #ID` và vệt di chuyển
  tối đa 30 vị trí gần nhất. `Nguoi ?` là phát hiện chưa có ID được xác nhận.
  Bộ nhớ track mất dấu khoảng 2 giây ở FPS nguồn; ID có thể đổi sau che khuất
  dài hoặc khi các người chồng lấn. Có thể tắt bằng `--tracking none`.
- Mọi mũ được vẽ hộp vàng và nhãn `Mu`, mọi vest được vẽ hộp cyan và nhãn
  `Vest`, kèm độ tin cậy. Nếu ghép được với người đã có track, nhãn thêm `#ID`
  của người đó. Đây là ID chủ thể được ghép, không phải track riêng của PPE.
- **Xanh**: người có cả mũ và áo. **Đỏ**: thiếu mũ, thiếu áo hoặc thiếu cả hai,
  với nhãn `LOI: THIEU MU`, `LOI: THIEU AO`, `LOI: THIEU MU + AO`.
  Đây là trạng thái theo phát hiện trong từng frame: che khuất hoặc model bỏ
  sót PPE có thể tạo khung đỏ; tracking giữ ID nhưng trạng thái PPE chưa được
  xác nhận/làm mượt qua nhiều frame.
- `<output>.csv` lưu toàn bộ phát hiện (mũ=0, áo=1, người=100).
  `<output>_safety.csv` lưu một dòng/người/frame với `has_helmet`, `has_vest`
  và `status`. Cả hai CSV thêm cột `track_id` (`-1` khi chưa gán/không track).
  `person_index` chỉ là số thứ tự trong frame; dùng `track_id` để theo dõi người.
  Các bộ đếm là số lượt người qua các frame, không phải số người duy nhất.
- Metadata người có secondary classification: mask `0` = đủ, `1` = thiếu mũ,
  `2` = thiếu áo, `3` = thiếu cả hai; nhãn `ppe:ok`, `ppe:missing_helmet`,
  `ppe:missing_vest`, `ppe:missing_helmet_and_vest`.
- Script `run_ppe_safety_video.sh` mặc định bật `--analysis-board 1` để xuất
  `<output>_board.png` (snapshot cuối) và `<output>_board.mp4` (mỗi frame video
  có một frame board tương ứng). Board hiển thị topology, hàng đợi, FPS và
  latency thực tế của các node, kể cả `person_bytetrack`. Không cần màn hình
  hoặc RTMP server. Dùng `--analysis-board 0` để tắt xuất board. FPS trên board
  là tốc độ xử lý thực tế, khác FPS phát lại của file MP4.

Kiểm thử logic ghép/màu/đồng bộ và kiểm thử thực tế trên GPU:

```bash
cmake --build build --target test_ppe_safety -j4
ctest --test-dir build -R test_ppe_safety --output-on-failure
python3 tests/test_ppe_safety_video.py --build-dir build
```

Test GPU cần `numpy`, `cv2`, `onnxruntime` và `ffprobe`; chạy 12 frame qua
hai nhánh, kiểm tra CSV/video, ID người liên tục, video/ảnh analysis board và
so tọa độ người với ONNX gốc trên 3 frame. Test C++ kiểm tra thêm khung mũ/vest,
lọc tracking theo lớp, thay đổi thứ tự detections và khôi phục ID sau mất dấu ngắn.

## Gửi event người không an toàn lên HeraMind qua MQTT

Thêm `--mqtt-host` là pipeline mọc thêm nhánh broker sau node an toàn PPE:

```text
... -> ByteTrack -> PPE safety -> cvedix_ppe_event_node -> MQTT
                                (MP4/CSV vẫn ghi song song)
```

Mỗi người được xác nhận vi phạm sinh một message gồm **ảnh crop của chính
người đó** cùng bbox, `track_id` và `event_id`:

```bash
./build/bin/ppe_video_sample \
  --backend tensorrt \
  --video data/videos/16.22.09.mp4 \
  --model data/models/yolov11n_ppe_detection_fp16.engine \
  --person-model data/models/yolov11-cetection_fp16.engine \
  --labels configs/ppe_labels.txt --tracking bytetrack \
  --output output/ppe/safety_mqtt.mp4 \
  --mqtt-host 192.168.1.50 --mqtt-port 1883 \
  --mqtt-topic heramind/ppe/events --camera-id camera-01 \
  --event-archive-dir output/ppe/events
```

Payload theo hợp đồng HeraMind: một **mảng JSON** phân biệt bằng `$id`, gồm
`event-ppe-violation` (event_id, instance_id, ref_tracking_id, object_class,
`location` chuẩn hoá [0,1]), các `attribute` (`has_helmet`, `has_vest`,
`missing_helmet`, `missing_vest`, `ppe_status`, `person_confidence`, `track_id`,
`frame_index`) và `crop` (ảnh JPEG base64 kèm `ref_event_id`, `location`).
Bbox pixel gốc vẫn nằm trong event item (`frame_width`/`frame_height`) để bên
nhận tự cắt lại crop nếu cần.

`ref_tracking_id` là định danh **ổn định** cho cặp `(camera_id, track_id)`,
không phải UUID ngẫu nhiên: HeraMind dùng nó để nối event với attribute và crop
của cùng một người qua nhiều lần vi phạm.

Chi tiết cờ dòng lệnh, biến môi trường `HERAMIND_*`, bảng metric và script đăng
ký HeraMind nằm trong [integrations/heramind_ppe/README.md](../integrations/heramind_ppe/README.md).

Ràng buộc: MQTT là **bắt buộc** khi đã chỉ định `--mqtt-host` — không kết nối
được broker trong 5 giây thì sample thoát ngay. Cần `--tracking bytetrack`
(event cần `track_id`) và `--person-model` (cần bbox người để crop). Sample trả
mã thoát `2` nếu có event lỗi. Unit test payload không cần model, GPU hay broker:

```bash
cmake --build build --target test_heramind_event -j4
ctest --test-dir build -R test_heramind_event --output-on-failure
```

## Build và chạy

### Đồng phục lao động (áo + quần)

Với model hai class có `0 = áo`, `1 = quần`, dùng `--uniform`. Chế độ này dùng
`configs/uniform_labels.txt`, tự bật person detector TensorRT và ByteTrack, ghép
áo vào vùng thân trên/quần vào vùng thân dưới của từng người, rồi ghi trạng thái
trên khung người và trong CSV riêng:

```bash
bash scripts/run_ppe_video.sh --uniform \\
  --video 'data/videos/2026-09-25 16-12-30.mp4' \\
  --model data/models/qdp_ppe_detection.engine \\
  --output output/ppe/2026-09-25_16-12-30_uniform.mp4
```

`<output>_safety.csv` ở chế độ này có `has_shirt`, `has_pants`, `status`.
Trạng thái thể hiện phát hiện theo từng frame, không khẳng định chất lượng hoặc
mức chứng nhận của trang phục.

Máy cần dependencies của SDK trong `docs/DEVELOPMENT.md`, NVIDIA GPU,
CUDA, TensorRT và OpenCV có video decoder/encoder. Engine hiện tại được build
trên RTX 3080, TensorRT 10.9. Nếu thay môi trường GPU/TensorRT, cần kiểm tra
tương thích và có thể phải build lại engine từ ONNX.
Không cần RTMP server hoặc màn hình. Bật TensorRT trong build SDK hiện có:

```bash
cmake -S . -B build -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_CUDA=ON
bash scripts/run_ppe_video.sh
```

Nếu cấu hình mới (chỉ cần thực hiện một lần):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCVEDIX_WITH_CUDA=ON -DCVEDIX_WITH_TRT=ON -DCVEDIX_WITH_LLM=OFF \
  -DCVEDIX_BUILD_TESTS=ON \
  -DMEDIATOOLKIT_LIB=/absolute/path/to/libMediaToolKit.a
bash scripts/run_ppe_video.sh
```

`MediaToolKit` là dependency hiện có của SDK. Nếu CMake cache trỏ tới thư
viện đã bị di chuyển, cấu hình lại `MEDIATOOLKIT_LIB` với đường dẫn thực tế.

Kiểm tra nhanh 30 frame hoặc điều chỉnh tham số:

```bash
bash scripts/run_ppe_video.sh --max-frames 30 --output output/ppe/smoke_trt.mp4
bash scripts/run_ppe_video.sh \
  --backend tensorrt \
  --video data/videos/16.22.09.mp4 \
  --model data/models/yolov11n_ppe_detection_fp16.engine \
  --labels configs/ppe_labels.txt \
  --conf 0.25 --nms 0.45 \
  --output output/ppe/custom_trt.mp4

# Đối chiếu CPU bằng model ONNX gốc
bash scripts/run_ppe_video.sh --backend onnx --max-frames 30 \
  --output output/ppe/compare_cpu.mp4
```

Mặc định xử lý hết video rồi tự thoát. `--max-frames 0` cũng xử lý hết.
Ctrl+C kết thúc sau frame đang xử lý và đóng MP4. Sample từ chối ghi đè
output đã tồn tại; dùng tên mới khi chạy lại. `CVEDIX_BUILD_DIR` chọn build
directory khác; `CVEDIX_BUILD_JOBS` chọn số luồng build (mặc định 4).
`ppe_video_sample` tự build kèm plugin `libtrt_yolov11.so` khi TensorRT được bật;
script thêm `build/libs` vào đường dẫn nạp plugin. Nếu dùng build CPU-only,
phải chọn `--backend onnx`. `--model` cần đuôi `.engine` cho TensorRT hoặc
`.onnx` cho ONNX; model và đường dẫn output mặc định được chọn theo backend.

## Phát live (RTSP nội bộ hoặc RTMP lên media server)

Hai nhánh `des` là **anh em** với sink MP4. Khi có nhánh live mà không truyền
`--output`, sample **không ghi MP4/CSV** — chỉ phát luồng; truyền thêm `--output`
thì vừa phát vừa ghi. Bỏ qua cờ live thì pipeline giữ nguyên như trước.

**RTMP lên media server** (ZLMediaKit, nginx-rtmp, ...) — `--rtmp-url` được
dùng **nguyên văn**, không thêm hậu tố `_<channel>`. Media server phát lại qua
RTSP/HTTP/HLS tuỳ cấu hình; ví dụ ZLMediaKit nhận RTMP ở 1935 và phát lại RTSP
ở 8554:

```bash
bash scripts/run_ppe_video.sh \
  --person-model data/models/yolov11-cetection_fp16.engine \
  --rtmp-url rtmp://127.0.0.1:1935/live/ppe_video \
  --rtmp-resolution 1280x720 --rtmp-bitrate 2048
# xem lại: rtsp://127.0.0.1:8554/live/ppe_video
```

Demo chạy liên tục, vừa phát luồng vừa đẩy event lên HeraMind (không ghi file):

```bash
bash scripts/run_ppe_video.sh \
  --person-model data/models/yolov11-cetection_fp16.engine \
  --tracking bytetrack --loop 1 \
  --rtmp-url rtmp://127.0.0.1:1935/live/ppe_video \
  --rtmp-encoder nvh264enc --rtmp-resolution 1280x720 --rtmp-bitrate 2048 \
  --mqtt-host 127.0.0.1 --mqtt-port 1883 \
  --mqtt-topic heramind/ppe/events --camera-id camera-01
# Ctrl+C để dừng. Bỏ --rtmp-url để chỉ đẩy MQTT, bỏ cụm --mqtt-* để chỉ phát luồng.
```

**RTSP server nội bộ** — node tự mở server, không cần media server ngoài:

```bash
bash scripts/run_ppe_video.sh --rtsp-port 8554 --rtsp-name ppe_video
# xem tại rtsp://<host>:8554/ppe_video
```

- `--rtmp-resolution WxH`: mặc định bằng kích thước frame nguồn. Đặt nhỏ hơn khi
  mã hoá bằng `x264enc` để giảm CPU, hoặc dùng `--rtmp-encoder nvh264enc` khi có
  NVENC. Node đặt caps của `appsrc` theo đúng kích thước này, nên không được để
  trống.
- **Chọn encoder theo tải máy.** `x264enc` là mặc định nhưng chạy trên CPU cùng
  lúc với hai model TensorRT: đo trên máy này ở 1280x720, nó không giữ nổi nhịp
  nguồn và luồng tụt còn 1-2 fps (hình vẫn sạch nhờ rơi frame trước encoder,
  nhưng thưa tới mức ZLMediaKit cắt kết nối publisher và node phải dựng lại
  pipeline). `--rtmp-encoder nvh264enc` đẩy mã hoá xuống GPU và giữ đúng 15 fps
  của nguồn. Dùng `nvh264enc` cho demo chạy dài.
- Queue của nhánh live giữ **1 frame**: encoder chậm thì rơi frame chứ không
  dồn backlog làm luồng trễ dần. Node RTMP tự phát hiện pipeline lỗi (server
  tắt, mất kết nối) qua bus GStreamer và dựng lại sau 3 giây.
- Chỉ được rơi frame **trước** encoder. Queue sau `flvmux` giữ nguyên
  backpressure: buffer ở đó đã là FLV tag, rơi một cái là đứt chuỗi tham chiếu
  của H.264 và hình vỡ tan cho tới keyframe kế tiếp. Encoder được ghim
  `key-int-max=30` (`x264enc`) hoặc `gop-size=30` (`nvh264enc`/`nvh265enc`) để
  giới hạn thời gian vỡ còn khoảng 2 giây; mặc định của cả hai là 250 frame.
- Định dạng ép về `I420`: nếu để `videoconvert` tự chọn, nó và `x264enc` thống
  nhất dùng Y444 và stream ra profile **High 4:4:4 Predictive** — nhiều
  player/trình duyệt không giải mã được. Stream hiện tại là Constrained
  Baseline / yuv420p.
- Caps của `appsrc` lấy FPS thật của nguồn, không hardcode: FPS đó là timebase
  đánh PTS, lệch với PTS thì `flvmux` hiểu sai tốc độ luồng.
- `--loop 1` cho video tự quay lại từ đầu khi hết, để chạy demo liên tục:
  `frame_index` và timestamp nguồn vẫn tăng đơn điệu qua các vòng (node event
  đo `confirm-frames`/`cooldown-ms` theo `frame_index`, không được reset), và
  pipeline **không** dựng lại nên tracker giữ ID, người xem thấy một luồng
  liền mạch. Dùng cùng `--max-frames N` để chặn tổng số frame, Ctrl+C để dừng.
  Kết hợp `--loop 1` với `--output` sẽ ghi một file MP4 dài vô hạn.
- `--analysis-board 1` cần `--output` vì board ghi PNG/MP4 cạnh file output.
- Biến môi trường thay cờ: `CVEDIX_RTMP_URL`, `CVEDIX_RTMP_ENCODER`,
  `CVEDIX_RTMP_BITRATE`, `CVEDIX_RTMP_RESOLUTION`, `CVEDIX_RTSP_PORT`,
  `CVEDIX_RTSP_NAME`.
- Giới hạn: sample đọc video offline nên pipeline chạy **nhanh hơn thời gian
  thực**; luồng live vì thế bị nén thời gian. Muốn đúng nhịp thời gian thực cần
  nguồn camera (`cvedix_rtsp_src_node`).

## Kết quả và giới hạn

- `output/ppe/16.22.09_ppe_trt.mp4`: video có hộp và nhãn nhận diện, giữ kích
  thước frame nguồn, không có âm thanh.
- `output/ppe/16.22.09_ppe_trt.csv`: mỗi phát hiện một dòng, gồm frame index
  (từ 0), timestamp nguồn (ms), class ID, confidence, x/y/width/height theo
  pixel ảnh nguồn. Frame không có phát hiện không có dòng CSV.
- Chuẩn hóa RGB / 255, letterbox 640×640, giải mã head theo số lớp thực tế,
  NMS riêng từng lớp, đổi tọa độ trở lại ảnh gốc.
- Chỉ một frame đang xử lý trong pipeline để tránh tràn queue và rớt frame.
  TensorRT suy luận trên GPU; đọc video, chuẩn hóa ảnh, NMS, OSD và ghi MP4 vẫn
  dùng CPU. FPS toàn pipeline vì thế khác benchmark riêng engine.
- MP4 xuất dùng FPS cố định do decoder báo. Với video nguồn có FPS thay đổi,
  thời lượng xuất có thể khác nguồn; timestamp CSV giữ mốc thời gian decoder
  của nguồn. Sample không hỗ trợ model end-to-end/NMS hoặc input khác 640×640.
- Khi chạy, console in backend đang dùng và engine in `Classes: 2, Boxes: 8400`.
  Luồng ONNX giữ tên output mặc định `16.22.09_ppe.mp4` / `.csv` như trước.

## Kiểm tra hồi quy bộ giải mã

```bash
cmake --build build --target test_yolov11_output -j4
ctest --test-dir build -R test_yolov11_output --output-on-failure
```

Test kiểm tra head PPE hai lớp, head COCO, hai thứ tự chiều đầu ra và từ chối
tensor sai định dạng. Kiểm chứng toàn luồng bằng lệnh smoke phía trên.

Kiểm tra hồi quy pipeline TensorRT (cần GPU, `ffprobe` và đúng video/model PPE
trong workspace):

```bash
python3 tests/test_ppe_video.py --build-dir build
```

Test chạy 30 frame với cả hai backend vào thư mục tạm, kiểm tra không mất frame,
đủ hai lớp và tọa độ tương đồng. Test cũng bắt lỗi NMS chọn hộp không ổn định
khi nhiều anchor có cùng confidence sau làm tròn FP16. Không đăng ký test này
vào CTest mặc định vì cần GPU và dữ liệu cục bộ.
