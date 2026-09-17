# Đẩy event PPE lên HeraMind

Bộ asset này đăng ký camera PPE của Core Runtime với một instance HeraMind, để
ảnh crop người không an toàn cùng bbox / tracking_id / event_id hiện lên
dashboard.

Luồng dữ liệu:

```
ppe_video_sample                mosquitto                HeraMind
─────────────────               ─────────                ────────
PPE safety node                     │                        │
   └─ cvedix_ppe_event_node         │                        │
        ├─ crop JPEG (person)  ─────┤                        │
        ├─ bbox chuẩn hoá [0,1]     │                        │
        ├─ track_id → UUID          │                        │
        └─ event_id (UUID)     ──► publish ──► telemetry ──► transform.js ──► metric
```

Payload là một **mảng JSON** phân biệt bằng `$id`:
`event-ppe-violation` (sự kiện), `attribute` (từng thuộc tính),
`crop` (ảnh bằng chứng base64). Xem
[`device-type.json`](./device-type.json) để có payload mẫu đầy đủ, hoặc
`nodes/broker/cvedix_heramind_event_payload.h` để biết nguồn sinh payload.

## 1. Chạy sample với MQTT bật

MQTT là **bắt buộc** khi đã chỉ định `--mqtt-host`: nếu không kết nối được
broker trong 5 giây, sample thoát ngay thay vì chạy im lặng mà không gửi event.

```bash
./build/bin/ppe_video_sample \
  --backend tensorrt \
  --video data/videos/16.22.09.mp4 \
  --model data/models/yolov11n_ppe_detection_fp16.engine \
  --person-model data/models/yolov11-cetection_fp16.engine \
  --labels configs/ppe_labels.txt \
  --tracking bytetrack \
  --output /tmp/ppe.mp4 \
  --mqtt-host 192.168.1.50 --mqtt-port 1883 \
  --mqtt-topic heramind/ppe/events \
  --camera-id camera-01 \
  --event-archive-dir /tmp/ppe_events
```

Có thể đặt qua biến môi trường thay vì cờ dòng lệnh — tiện khi chạy trong
service:

| Biến | Ý nghĩa |
|------|---------|
| `HERAMIND_MQTT_HOST` | Host broker |
| `HERAMIND_MQTT_PORT` | Cổng broker, mặc định `1883` |
| `HERAMIND_MQTT_TOPIC` | Topic publish |
| `HERAMIND_CAMERA_ID` | `instance_id` bên HeraMind |
| `HERAMIND_MQTT_USERNAME` / `HERAMIND_MQTT_PASSWORD` | Thông tin đăng nhập; **không bao giờ ghi ra log** |
| `HERAMIND_MQTT_CA_FILE` / `_CERT_FILE` / `_KEY_FILE` | TLS |

Ràng buộc bắt buộc, sample sẽ từ chối chạy nếu vi phạm:

- `--tracking bytetrack` — event cần `track_id`; chế độ `none` khiến mọi event
  bị bỏ qua (`skipped (no track id)` trong báo cáo cuối).
- `--person-model` — phải có nhánh người song song thì mới có bbox để crop.
- `--camera-id` và `--mqtt-topic` không được rỗng; topic không được chứa `+`/`#`.

Các cờ tinh chỉnh:

| Cờ | Mặc định | Ý nghĩa |
|----|----------|---------|
| `--confirm-frames N` | `3` | Số frame liên tiếp cùng trạng thái vi phạm trước khi phát event |
| `--cooldown-ms N` | `10000` | Khoảng nghỉ tối thiểu giữa hai event của cùng một track |
| `--event-archive-dir DIR` | (không) | Lưu `.json` + `.jpg` mỗi event xuống đĩa trước khi gửi |

Báo cáo cuối có dạng:

```
HeraMind events: 4 generated, 4 queued, 4 published, 4 acknowledged, 0 errors, 7 skipped (no track id)
```

`queued` là số message đã đưa vào hàng đợi publish; `acknowledged` là số broker
đã xác nhận ở QoS 1. Sample trả mã thoát `2` nếu có event lỗi.

## 2. Đăng ký với HeraMind

```bash
export HERAMIND_API_KEY=...            # hoặc HERAMIND_TOKEN
export HERAMIND_MQTT_HOST=192.168.1.50
export HERAMIND_MQTT_TOPIC=heramind/ppe/events
export HERAMIND_CAMERA_ID=camera-01    # phải trùng --camera-id của sample
bash integrations/heramind_ppe/setup.sh
```

Script tạo/cập nhật 4 thứ, chạy lại nhiều lần vẫn an toàn:

| Bước | Đối tượng |
|------|-----------|
| `POST /device-types` | `cvedix_ppe_violation_analytics` |
| `POST /devices` | `camera-01`, adapter `mqtt`, `telemetry_topic` = topic publish |
| `POST /brokers` | Broker để HeraMind subscribe |
| `POST /automations` | `transform.js`, scope theo device, prefix `cvedix_ppe` |
| `POST /dashboards` | Dashboard "CVEDIX PPE - An toàn lao động" |

> `HERAMIND_CAMERA_ID` **phải trùng** `--camera-id` của sample. Giá trị đó
> chính là `instance_id` trong payload, và transform chỉ nhận event khớp scope
> device đã đăng ký.

## 3. Metric trên dashboard

`transform.js` làm phẳng mảng `$id` thành các metric sau:

| Metric | Kiểu | Nguồn |
|--------|------|-------|
| `ppe_violation_seen` | bool | Có ít nhất một `event-ppe-violation` trong message |
| `ppe_violation_seen_numeric` | 0/1 | Bản số của metric trên, dùng cho line chart |
| `ppe_violation_count` | số | Số event trong một message |
| `ppe_status` | chuỗi | `ppe:ok`, `ppe:missing_helmet`, `ppe:missing_vest`, `ppe:missing_helmet_and_vest` |
| `missing_ppe` | chuỗi | Danh sách cách nhau bởi dấu phẩy, ví dụ `helmet,vest` |
| `missing_ppe_count` | số | Số loại PPE còn thiếu |
| `has_helmet` / `has_vest` | bool | Trạng thái từng loại |
| `person_confidence` | số | Điểm phát hiện người |
| `track_id` | số | Track id nội bộ của ByteTrack |
| `tracking_id` | UUID | `ref_tracking_id` — ổn định theo `(camera, track)` |
| `event_id` | UUID | Định danh sự kiện |
| `event_time` | ISO 8601 | `system_datetime` |
| `event_timestamp_ms` | ms | Thời gian nguồn, tính từ `frame_index / source_fps` |
| `system_timestamp` | ms | Thời gian hệ thống lúc phát |
| `bbox_x/y/width/height` | [0,1] | `location` đã chuẩn hoá theo khung hình gốc |
| `crop_image` | data URI | Ảnh crop người, `data:image/jpeg;base64,...` |
| `crop_confidence`, `crop_timestamp_ms`, `crop_event_id` | | Metadata của ảnh |

### Vì sao `tracking_id` là UUID chứ không phải số

HeraMind dùng `ref_tracking_id` để nối một event với các `attribute` và `crop`
của nó. `generate_uuid()` của Core Runtime là ngẫu nhiên nên không dùng được,
còn `track_id` dạng số sẽ trùng giữa các camera. Vì vậy
`stable_tracking_id()` băm FNV-1a chuỗi `"<camera_id>:<track_id>"` thành một
định danh hình dạng UUID, **ổn định** cho cùng một người qua mọi event. Đây là
định danh ổn định, không phải UUID bảo mật.

## 4. Kiểm thử cục bộ

Không cần HeraMind để xác nhận đường publish hoạt động:

```bash
# Broker tạm
printf 'listener 1883 127.0.0.1\nallow_anonymous true\n' > /tmp/mosq.conf
mosquitto -c /tmp/mosq.conf -d

# Bắt message
mosquitto_sub -h 127.0.0.1 -t 'heramind/ppe/#' -v

# Chạy sample ở terminal khác với --mqtt-host 127.0.0.1
```

Unit test cho payload builder (không cần model, GPU hay broker):

```bash
cmake --build build --target test_heramind_event && ./build/tests/test_heramind_event
```

## Xử lý sự cố

| Triệu chứng | Nguyên nhân thường gặp |
|-------------|------------------------|
| Thoát ngay với "Cannot connect to MQTT broker" | Sai host/port, broker chưa chạy, hoặc firewall |
| `0 generated`, `N skipped (no track id)` | Đang chạy `--tracking none` |
| `queued > acknowledged` | Broker nhận chậm hoặc ngắt kết nối; xem `--event-archive-dir` để lấy event đã sinh |
| Dashboard trống dù event đã tới | `HERAMIND_CAMERA_ID` khác `--camera-id`, nên scope transform không khớp |
| Ảnh không hiện | Broker chặn payload lớn (mặc định Mosquitto 256 MB); crop JPEG ~85% chất lượng thường 10–40 KB |
