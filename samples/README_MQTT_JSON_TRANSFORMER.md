# Hướng dẫn Custom JSON Transformer cho MQTT Broker

## Tổng quan

File `rknn_rtsp_tracking_mqtt_sample.cpp` chứa ví dụ về cách tùy chỉnh cấu trúc JSON trước khi gửi qua MQTT.

## Cấu trúc JSON Output

Transformer function `custom_json_transformer` chuyển đổi JSON mặc định từ broker sang định dạng:

```json
[{
  "events": [
    {
      "best_thumbnail": {
        "confidence": 0.712,
        "image": "base64_string_or_empty",
        "position": {"x": 0.298, "y": 0.269, "width": 0.162, "height": 0.297},
        "timestamp": 2746.29
      },
      "date": "Sun Nov 16 15:48:28 2025",
      "extra": {
        "bbox": {"x": 0.298, "y": 0.269, "width": 0.162, "height": 0.297},
        "class": "Person",
        "current_entries": 0,
        "external_id": "uuid",
        "total_hits": 68661,
        "track_id": "PersonTracker_68661"
      },
      "frame_id": 68661,
      "frame_time": 2746.2899002141,
      "id": "uuid",
      "image": "base64_string_or_empty",
      "instance_id": "uuid",
      "label": "Person movement in area",
      "system_date": "2025-11-16T07:48:28Z",
      "tracks": [...],
      "type": "area_enter",
      "zone_id": "uuid",
      "zone_name": "Intrusion area 1"
    }
  ],
  "frame_id": 68661,
  "frame_time": 2746.2899002141,
  "system_date": "Sun Nov 16 15:48:28 2025",
  "system_timestamp": "1763279308605"
}]
```

## Các Helper Functions

### 1. `generate_uuid()`
Tạo UUID v4 format string (ví dụ: `a0deda9f-efae-87e4-3f8f-79d21e3d3ec8`)

### 2. `format_date_string()`
Format date theo format: `"Sun Nov 16 15:48:28 2025"`

### 3. `format_iso_date_string()`
Format date theo ISO 8601: `"2025-11-16T07:48:28Z"`

### 4. `normalize_bbox(x, y, width, height, frame_width, frame_height)`
Chuyển đổi bbox từ pixel coordinates sang normalized coordinates (0-1)

## Cách Tùy Chỉnh

### Thay đổi Zone Information

```cpp
event["zone_id"] = "your-zone-id";  // Thay vì generate_uuid()
event["zone_name"] = "Your Zone Name";
```

### Thay đổi Event Type

```cpp
// Có thể thay đổi dựa trên logic tracking
if (is_entering_zone) {
    event["type"] = "area_enter";
} else if (is_exiting_zone) {
    event["type"] = "area_exit";
} else {
    event["type"] = "area_enter";  // Default
}
```

### Thêm Base64 Images

**Cách 1: Sử dụng Enhanced Broker**

Tạo custom broker node kế thừa từ `cvedix_json_mqtt_broker_node` và override `format_msg`:

```cpp
class custom_mqtt_broker_node : public cvedix_json_mqtt_broker_node {
protected:
    void format_msg(...) override {
        // Serialize với base64 images giống enhanced broker
        // Sau đó gọi json_transformer
    }
};
```

**Cách 2: Parse từ Enhanced Broker JSON**

Nếu sử dụng enhanced broker trước, có thể parse JSON từ enhanced broker trong transformer.

### Thay đổi Instance ID

```cpp
// Thay vì generate mỗi lần, có thể set static
static std::string instance_id = "your-instance-id";
```

### Thay đổi Label Format

```cpp
event["label"] = class_label + " detected in " + zone_name;
```

## Sử dụng trong Pipeline

```cpp
auto mqtt_broker = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
    "mqtt_broker_0",
    cvedix_nodes::cvedix_broke_for::NORMAL,
    50, 200,
    custom_json_transformer,  // Custom transformer
    my_mqtt_publisher
);
```

## Lưu ý

1. **Base64 Images**: JSON mặc định từ MQTT broker không có `crop_base64`. Nếu cần, phải tạo custom broker node.

2. **Performance**: Transformer được gọi cho mỗi frame, nên cần optimize nếu xử lý nhiều targets.

3. **Error Handling**: Transformer có try-catch để tránh crash, nếu lỗi sẽ trả về JSON gốc.

4. **Thread Safety**: Transformer được gọi từ broker thread, đảm bảo thread-safe nếu sử dụng shared data.

## Ví dụ Customization

### Thêm Custom Fields

```cpp
event["custom_field"] = "custom_value";
event["metadata"] = {
    {"camera_id", "cam_01"},
    {"location", "entrance"}
};
```

### Filter Targets

```cpp
// Chỉ giữ lại targets có confidence > 0.7
if (confidence > 0.7) {
    events.push_back(event);
}
```

### Modify Bbox Format

```cpp
// Thay vì normalized, có thể dùng absolute coordinates
json bbox;
bbox["x1"] = x;
bbox["y1"] = y;
bbox["x2"] = x + width;
bbox["y2"] = y + height;
```

