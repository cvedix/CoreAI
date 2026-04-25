# Hướng dẫn sử dụng `cvedix_ba_event_extraction_node` kết hợp với các node truyền tải

---

## 1. Tổng quan

`cvedix_ba_event_extraction_node` là node **trích xuất và chuẩn hóa** BA events (Business Analytics) thành JSON theo định dạng chuẩn. Node này implement **broker pattern chuẩn** của hệ thống:

```
BA nodes (intrusion, crossline, crowding...)
        │
        ▼ (handle_frame_meta: đẩy vào queue)
┌───────────────────────────────────────────────┐
│  cvedix_ba_event_extraction_node             │
│                                             │
│  format_msg()  → serialize ba_results → JSON │
│      │ (msg = JSON array)                     │
│  broker thread                               │
│      │ (broke_msg(msg))                      │
│      ▼                                      │
│  event_publisher callback ──────────────────┼──→ MQTT
│      │                                       │    Webhook
│      │                                       │    Kafka
│      │                                       │    SSE
└───────────────────────────────────────────────┘
```

### Cách hoạt động

| Method | Chạy trên | Nhiệm vụ |
|---|---|---|
| `handle_frame_meta()` | **Main thread** | Đẩy `frame_meta` vào queue |
| `format_msg()` | **Broker thread** | Serialize `ba_results` → JSON array, gán `msg` |
| `broke_msg()` | **Broker thread** | Gọi `event_publisher(msg)` để gửi đi |
| `push_event()` | **Bất kỳ thread** | Gửi ngay lập tức, bypass queue |

> **Khác với callback trực tiếp:** `event_publisher` được gọi trong `broke_msg()` bởi **broker thread** (thread riêng), không phải main thread — đảm bảo pipeline không bị blocking.

### 1.1 Các loại BA event được hỗ trợ

| BA Type | `$id` Schema | Loại |
|---|---|---|
| `CROWDING` | `event-crowd-detection` | Group event |
| `AREA_ENTER` | `event-area-enter` | Single-target |
| `AREA_EXIT` | `event-area-exit` | Single-target |
| `INTRUSION_START` | `event-intrusion` | Single-target |
| `INTRUSION_END` | `event-intrusion-end` | Single-target |
| `CROSSLINE` | `event-crossline` | Single-target |
| `STOP` | `event-stop` | Single-target |
| `UNSTOP` | `event-stop-end` | Single-target |
| `LOITERING` | `event-loitering` | Single-target |
| `LOITERING_END` | `event-loitering-end` | Single-target |
| `DWELL` | `event-dwelling` | Single-target |

### 1.2 Định dạng JSON đầu ra

`format_msg()` gộp tất cả events trong frame thành **JSON array**. Ví dụ nếu có 2 events trong 1 frame:

```json
[
  {
    "$id": "event-crossline",
    "$version": 1,
    "area_id": "zone-001",
    "area_name": "Crossline Zone",
    "event_id": "evt-uuid-123",
    "event_timestamp_ms": 1744000000000,
    "instance_id": "pipeline-uuid",
    "location": {
      "height": 100,
      "width": 50,
      "x": 10,
      "y": 20
    },
    "object_class": "person",
    "ref_tracking_id": "track-001",
    "crop_image": "base64...",    // nếu include_crop_images=true
    "system_datetime": "2026-04-07T10:30:00",
    "system_timestamp": 1744000000000
  },
  {
    "$id": "event-intrusion",
    "$version": 1,
    "area_id": "zone-002",
    "area_name": "Intrusion Zone",
    "event_id": "evt-uuid-456",
    "event_timestamp_ms": 1744000000500,
    "instance_id": "pipeline-uuid",
    "location": {
      "height": 110,
      "width": 55,
      "x": 80,
      "y": 30
    },
    "object_class": "car",
    "ref_tracking_id": "track-003",
    "system_datetime": "2026-04-07T10:30:00",
    "system_timestamp": 1744000000500
  }
]
```

**Group event (CROWDING)** — nhiều targets trong 1 event:
```json
[
  {
    "$id": "event-crowd-detection",
    "$version": 1,
    "area_id": "zone-crowd",
    "area_name": "Crowd Zone",
    "event_id": "evt-uuid-789",
    "event_timestamp_ms": 1744000001000,
    "targets": [
      {
        "location": {"height": 100, "width": 50, "x": 10, "y": 20},
        "object_class": "person",
        "ref_tracking_id": "track-001"
      },
      {
        "location": {"height": 110, "width": 55, "x": 80, "y": 30},
        "object_class": "person",
        "ref_tracking_id": "track-002"
      }
    ],
    "system_datetime": "2026-04-07T10:30:01",
    "system_timestamp": 1744000001000
  }
]
```

> **Thay đổi so với phiên bản trước:** Đầu ra là **JSON array** bao quanh tất cả events, gửi đi trong **một lần gọi** `broke_msg` thay vì gọi callback riêng cho từng event. Dùng `push_event(event_json)` để gửi từng event riêng lẻ.

---

## 2. Các cách tích hợp

### 2.1 Kết hợp MQTT (`cvedix_mqtt_broker_node`)

MQTT là giao thức **lightweight**, phù hợp cho các ứng dụng cần **low latency** và **high throughput**.

**Cấu trúc:**
```
ba_crossline ──frame_meta──► ba_event_extraction ──JSON array──► mqtt_broker
                         (format_msg)              (broke_msg)  (format_msg)
                                                                       │
                                                                       ▼
                                                                  mqtt_client.publish()
```

**Ví dụ đầy đủ:**
```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/broker/cvedix_ba_event_extraction_node.h"
#include "cvedix/nodes/broker/cvedix_mqtt_broker_node.h"
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // ─── 1. Khởi tạo MQTT client ───
    std::string mqtt_broker_host = "anhoidong.datacenter.cvedix.com";
    int mqtt_port = 1883;

    auto mqtt_client = std::make_unique<cvedix_utils::cvedix_mqtt_client>(
        mqtt_broker_host, mqtt_port,
        "ba_pipeline_" + std::to_string(std::time(nullptr)),
        60);
    mqtt_client->set_auto_reconnect(true, 5000);
    mqtt_client->connect("", "");

    // ─── 2. Tạo MQTT broker node ───
    auto mqtt_broker = std::make_shared<cvedix_nodes::cvedix_mqtt_broker_node>(
        "mqtt_broker",
        cvedix_nodes::cvedix_broke_for::NORMAL,
        50, 200,
        nullptr,  // json_transformer (dùng default)
        [&mqtt_client](const std::string& json) {
            if (mqtt_client->is_ready()) {
                mqtt_client->publish("analytics/events", json, 1, false);
            }
        });

    // ─── 3. Tạo BA event extraction node ───
    auto event_broker = std::make_shared<cvedix_nodes::cvedix_ba_event_extraction_node>(
        "ba_events",
        "my-pipeline-uuid-001",  // instance_id
        nullptr,                 // event_publisher (dùng broker pattern)
        false                    // include_crop_images
    );
    // Gắn vào mqtt_broker: event JSON → MQTT publish
    event_broker->attach_to({mqtt_broker});

    // ─── 4. Xây dựng pipeline ───
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/video/vehicle.mp4", 1.0);

    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector",
        "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
        "./cvedix_data/models/yolov11/onnx/labels.txt",
        0.45, 0.5, 0,
        cvedix_nodes::BackendType::ONNX);

    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");

    // Định nghĩa đường crossline
    cvedix_objects::cvedix_point start(0, 250);
    cvedix_objects::cvedix_point end(700, 220);
    std::map<int, cvedix_objects::cvedix_line> lines = {
        {0, cvedix_objects::cvedix_line(start, end)}
    };
    auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>(
        "ba_crossline", lines);

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

    // Kết nối pipeline
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    ba_crossline->attach_to({tracker});
    event_broker->attach_to({ba_crossline});  // BA events → mqtt_broker → MQTT
    osd->attach_to({event_broker});
    screen_des->attach_to({osd});

    // ─── 5. Chạy ───
    file_src->start();

    cvedix_utils::cvedix_analysis_board board({file_src});
    board.display(1, false);

    std::string wait;
    std::getline(std::cin, wait);

    // ─── 6. Cleanup ───
    file_src->detach_recursively();
    mqtt_client->disconnect();

    return 0;
}
```

**Compile:** Cần cài thư viện `libmosquitto-dev` (MQTT là mandatory dependency).

> **Hoặc dùng trực tiếp trong callback** (không qua broker chain):
> ```cpp
> auto event_broker = std::make_shared<cvedix_ba_event_extraction_node>(
>     "ba_events", "uuid",
>     [&](const std::string& json) {
>         mqtt_client->publish("events", json);  // gọi trong broke_msg
>     }, false);
> event_broker->attach_to({ba_crossline});
> ```

---

### 2.2 Kết hợp Webhook (`cvedix_webhook_broker_node`)

Webhook phù hợp khi cần gửi dữ liệu đến **web server** hoặc **REST API endpoint** với **custom headers** và **retry logic**.

**Cấu trúc:**
```
ba_intrusion ──frame_meta──► ba_event_extraction ──JSON──► webhook_broker
                              (format_msg)            (broke_msg) (format_msg)
                                                                       │
                                                                       ▼
                                                               HTTP POST /api/ba-events
```

**Ví dụ đầy đủ:**
```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_intrusion_detection_node.h"
#include "cvedix/nodes/broker/cvedix_ba_event_extraction_node.h"
#include "cvedix/nodes/broker/cvedix_webhook_broker_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/third_party/cpp_httplib/httplib.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::string webhook_url = "http://my-server:8080/api/ba-events";
    httplib::Headers headers = {
        {"Authorization", "Bearer my-token"},
        {"X-Device-Id", "camera-01"}
    };

    // ─── 1. Webhook broker node ───
    auto webhook_broker = std::make_shared<cvedix_nodes::cvedix_webhook_broker_node>(
        "webhook_broker",
        webhook_url,
        cvedix_nodes::cvedix_broke_for::NORMAL,
        50, 200,
        nullptr,  // json_transformer
        headers,
        5,   // timeout (seconds)
        2);  // retries

    // ─── 2. BA event extraction node ───
    auto event_broker = std::make_shared<cvedix_nodes::cvedix_ba_event_extraction_node>(
        "ba_events",
        "pipeline-uuid",
        nullptr,  // event_publisher (dùng broker pattern)
        false
    );
    // Gắn: event JSON → webhook_broker → HTTP POST
    event_broker->attach_to({webhook_broker});

    // ─── 3. Xây dựng pipeline ───
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/video/person.mp4", 1.0);

    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector",
        "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
        "./cvedix_data/models/yolov11/onnx/labels.txt",
        0.45, 0.5, 0,
        cvedix_nodes::BackendType::ONNX);

    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");

    // Định nghĩa vùng intrusion (polygon)
    std::vector<cvedix_objects::cvedix_point> polygon = {
        {100, 100}, {500, 100}, {500, 400}, {100, 400}
    };
    cvedix_objects::cvedix_polygon intrusion_zone(polygon);
    std::map<int, cvedix_objects::cvedix_polygon> zones = {{0, intrusion_zone}};
    auto ba_intrusion = std::make_shared<cvedix_nodes::cvedix_ba_intrusion_detection_node>(
        "ba_intrusion", zones, 1, 5.0f);

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

    // Kết nối pipeline
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    ba_intrusion->attach_to({tracker});
    webhook_broker->attach_to({ba_intrusion});   // detections → Webhook
    event_broker->attach_to({ba_intrusion});     // BA events → Webhook
    osd->attach_to({event_broker});
    screen_des->attach_to({osd});

    file_src->start();

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();

    return 0;
}
```

---

### 2.3 Kết hợp Kafka (`cvedix_kafka_broker_node`)

Kafka phù hợp cho các hệ thống **distributed**, cần **persist messages** và **multiple consumers**.

**Cấu trúc:**
```
ba_crowding ──frame_meta──► ba_event_extraction ──JSON──► kafka_broker
                            (format_msg)            (broke_msg) (format_msg)
                                                                 │
                                                                 ▼
                                                            Kafka topic
```

**Ví dụ đầy đủ:**
```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_area_crowding_node.h"
#include "cvedix/nodes/broker/cvedix_ba_event_extraction_node.h"
#include "cvedix/nodes/broker/cvedix_kafka_broker_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::string kafka_servers = "192.168.77.87:9092";
    std::string kafka_topic = "ba-events-topic";

    // ─── 1. Kafka broker node ───
    auto kafka_broker = std::make_shared<cvedix_nodes::cvedix_kafka_broker_node>(
        "kafka_broker",
        kafka_servers,
        kafka_topic,
        cvedix_nodes::cvedix_broke_for::NORMAL,
        50, 200);

    // ─── 2. BA event extraction node ───
    auto event_broker = std::make_shared<cvedix_nodes::cvedix_ba_event_extraction_node>(
        "ba_events",
        "pipeline-crowding-001",
        nullptr,  // event_publisher (dùng broker pattern)
        true      // include_crop_images để gửi kèm ảnh crowd
    );
    // Gắn: event JSON → kafka_broker → Kafka topic
    event_broker->attach_to({kafka_broker});

    // ─── 3. Xây dựng pipeline ───
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/video/crowd.mp4", 1.0);

    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector",
        "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
        "./cvedix_data/models/yolov11/onnx/labels.txt",
        0.45, 0.5, 0,
        cvedix_nodes::BackendType::ONNX);

    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");

    // Định nghĩa vùng crowding
    std::vector<cvedix_objects::cvedix_point> polygon = {
        {200, 150}, {600, 150}, {600, 450}, {200, 450}
    };
    cvedix_objects::cvedix_polygon crowd_zone(polygon);
    std::map<int, cvedix_objects::cvedix_polygon> zones = {{0, crowd_zone}};
    auto ba_crowding = std::make_shared<cvedix_nodes::cvedix_ba_area_crowding_node>(
        "ba_crowding", zones, 3, 5.0f);

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

    // Kết nối pipeline
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    ba_crowding->attach_to({tracker});
    kafka_broker->attach_to({ba_crowding});    // detections → Kafka
    event_broker->attach_to({ba_crowding});     // BA events → Kafka
    osd->attach_to({event_broker});
    screen_des->attach_to({osd});

    file_src->start();

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();

    return 0;
}
```

> **Lưu ý:** Cần compile với `-DCVEDIX_WITH_KAFKA=ON` và cài `librdkafka-dev`.

---

### 2.4 Kết hợp SSE (`cvedix_sse_broker_node`)

SSE (Server-Sent Events) phù hợp cho **web clients** cần nhận real-time events qua HTTP, ví dụ dashboard web.

**Cấu trúc:**
```
ba_crossline ──frame_meta──► ba_event_extraction ──JSON──► sse_broker
                            (format_msg)            (broke_msg) (format_msg)
                                                                   │
                                                                   ▼
                                                           Broadcast cho web clients
```

**Ví dụ đầy đủ:**
```cpp
#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/broker/cvedix_ba_event_extraction_node.h"
#include "cvedix/nodes/broker/cvedix_sse_broker_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"

int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // ─── 1. SSE broker node (lắng nghe port 8090, endpoint /ba-events) ───
    auto sse_broker = std::make_shared<cvedix_nodes::cvedix_sse_broker_node>(
        "sse_broker",
        cvedix_nodes::cvedix_broke_for::NORMAL,
        50, 200,
        8090,          // port
        "/ba-events"   // SSE endpoint
    );

    // ─── 2. BA event extraction node ───
    auto event_broker = std::make_shared<cvedix_nodes::cvedix_ba_event_extraction_node>(
        "ba_events",
        "pipeline-sse-001",
        nullptr,  // event_publisher (dùng broker pattern)
        false
    );
    // Gắn: event JSON → sse_broker → Broadcast
    event_broker->attach_to({sse_broker});

    // ─── 3. Xây dựng pipeline ───
    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
        "file_src", 0, "./cvedix_data/video/vehicle.mp4", 1.0);

    auto detector = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
        "yolo_detector",
        "./cvedix_data/models/yolov11/onnx/yolo11n.onnx",
        "./cvedix_data/models/yolov11/onnx/labels.txt",
        0.45, 0.5, 0,
        cvedix_nodes::BackendType::ONNX);

    auto tracker = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("sort_tracker");

    cvedix_objects::cvedix_point start(0, 250);
    cvedix_objects::cvedix_point end(700, 220);
    std::map<int, cvedix_objects::cvedix_line> lines = {
        {0, cvedix_objects::cvedix_line(start, end)}
    };
    auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>(
        "ba_crossline", lines);

    auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");
    auto screen_des = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des", 0);

    // Kết nối pipeline
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    ba_crossline->attach_to({tracker});
    sse_broker->attach_to({ba_crossline});    // detections → SSE
    event_broker->attach_to({ba_crossline});   // BA events → SSE
    osd->attach_to({event_broker});
    screen_des->attach_to({osd});

    file_src->start();

    std::cout << "SSE server listening at http://0.0.0.0:8090/ba-events" << std::endl;
    std::cout << "Test: EventSource('http://localhost:8090/ba-events')" << std::endl;

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();

    return 0;
}
```

**Test SSE bằng trình duyệt:**
```javascript
const es = new EventSource('http://localhost:8090/ba-events');
es.onmessage = (e) => {
    const events = JSON.parse(e.data);  // JSON array
    events.forEach(evt => console.log('BA Event:', evt));
};
```

---

## 3. So sánh các phương thức truyền tải

| Tiêu chí | MQTT | Webhook | Kafka | SSE |
|---|---|---|---|---|
| **Độ trễ** | Rất thấp | Thấp | Thấp | Thấp |
| **Persistency** | Không | Không | Có | Không |
| **Multi-consumer** | Có | Không | Có (topic) | Có |
| **Web browser** | Không (cần MQTT.js) | Có (REST) | Không | Có (原生) |
| **Authentication** | Username/Password | Bearer Token, API Key | SASL | Không |
| **Retry** | Tự động reconnect | Có (built-in) | Có (acks) | Không |
| **Queue size** | Tùy broker | Limited | Unlimited | Không |
| **Thêm dependency** | libmosquitto | cpp_httplib | librdkafka | asio |
| **Phù hợp cho** | IoT, mobile | REST API, serverless | Distributed systems | Web dashboard |

---

## 4. Lưu ý quan trọng

### 4.1 Callback thread-safety
`event_publisher` callback có thể được gọi từ **broking thread** khác với main thread. Nếu dùng shared resources (MQTT client, HTTP client), cần dùng `std::mutex` hoặc `std::atomic` để bảo vệ.

### 4.2 Queue management
`cvedix_ba_event_extraction_node` có thể gọi callback **nhiều lần mỗi frame** (nếu có nhiều BA events). Các broker nodes có queue với configurable thresholds:
- `broking_cache_warn_threshold` (default: 50) — cảnh báo khi queue vượt
- `broking_cache_ignore_threshold` (default: 200) — bỏ qua messages khi queue tràn

### 4.3 include_crop_images
- `false` (default): JSON nhẹ, chỉ chứa metadata
- `true`: Thêm `crop_image` (base64 JPEG, quality=85) cho từng target. **Cẩn thận** với bandwidth và message size.

### 4.4 Sử dụng nhiều transport cùng lúc
Có thể gắn **nhiều callback** hoặc dùng pattern observer:

```cpp
auto event_broker = std::make_shared<cvedix_ba_event_extraction_node>(
    "ba_events",
    "pipeline-uuid",
    nullptr,  // chưa set callback
    false);

// Callback 1: MQTT
event_broker->set_event_publisher([&](const std::string& json) {
    mqtt_client->publish("analytics/mqtt", json, 1, false);
});

// Nếu cần nhiều hơn 1 callback, dùng wrapper:
struct MultiPublisher {
    std::vector<std::function<void(const std::string&)>> publishers;
    void add(std::function<void(const std::string&)> p) { publishers.push_back(p); }
    void operator()(const std::string& json) {
        for (auto& p : publishers) p(json);
    }
};

MultiPublisher multi;
multi.add([&](const std::string& j){ mqtt_client->publish("a", j); });
multi.add([&](const std::string& j){ kafka_producer->push(j); });
multi.add([&](const std::string& j){ sse_server->broadcast(j); });

event_broker->set_event_publisher(multi);
```

---

## 5. API Reference

### Constructor
```cpp
cvedix_ba_event_extraction_node(
    std::string node_name,                                    // Tên node
    std::string instance_id = "",                              // Pipeline UUID
    std::function<void(const std::string&)> event_publisher,   // Callback (null = dùng broker pattern)
    bool include_crop_images = false                            // Có gửi ảnh crop không
);
```

### Methods
```cpp
// Đặt callback publish (sau khi tạo)
void set_event_publisher(std::function<void(const std::string&)> publisher);

// Đặt instance_id (sau khi tạo)
void set_instance_id(const std::string& id);

// Gửi ngay lập tức, bypass broker queue (SSE, webhook)
void push_event(const std::string& event_json);

// Gắn vào BA node(s) hoặc transport broker node
void attach_to(const std::vector<std::shared_ptr<cvedix_node>>& nodes);
```

### Broker pattern (cách khuyến nghị)
```
attach_to({mqtt_broker})       → event JSON → MQTT publish
attach_to({webhook_broker})    → event JSON → HTTP POST
attach_to({kafka_broker})     → event JSON → Kafka topic
attach_to({sse_broker})       → event JSON → Broadcast
```

### Callback pattern (gọi trực tiếp, không qua broker chain)
```cpp
auto broker = std::make_shared<cvedix_ba_event_extraction_node>(
    "ba_events", "uuid",
    [&](const std::string& json) {
        mqtt_client->publish("events", json);  // gọi trong broker thread
    }, false);
```

### Inheritance
Kế thừa từ `cvedix_msg_broker_node`, nên có đầy đủ lifecycle methods: `start()`, `detach()`, `detach_recursively()`.
