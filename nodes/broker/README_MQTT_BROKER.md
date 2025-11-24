# JSON MQTT Broker Node

## Tổng quan

`cvedix_json_mqtt_broker_node` là một node trung gian (transport layer) cho phép:
- Serialize `frame_meta` thành JSON
- Custom cấu trúc JSON theo nhu cầu
- Gửi JSON qua MQTT thông qua user-provided publisher function

Node này hoạt động như một "người vận chuyển" - chỉ chịu trách nhiệm serialize và gọi publisher function, không phụ thuộc vào thư viện MQTT cụ thể.

## Tính năng

1. **Custom JSON Transformation**: Cho phép user transform JSON trước khi gửi
2. **Flexible MQTT Publishing**: User tự implement MQTT publisher function
3. **Asynchronous Processing**: Không chặn pipeline
4. **No MQTT Library Dependency**: Không phụ thuộc vào thư viện MQTT cụ thể

## Cách sử dụng

### 1. Tạo MQTT Publisher Function

User cần implement function để publish JSON lên MQTT broker:

```cpp
void my_mqtt_publisher(const std::string& json_message) {
    // Sử dụng MQTT client library của bạn
    // Ví dụ với Paho MQTT:
    static mqtt::async_client client("tcp://localhost:1883", "client_id");
    auto msg = mqtt::make_message("videopipe/data", json_message);
    msg->set_qos(1);
    client.publish(msg);
    
    // Hoặc với thư viện khác:
    // mosquitto_publish(mosq, NULL, "videopipe/data", json_message.length(), json_message.c_str(), 1, false);
}
```

### 2. Tạo JSON Transformer Function (Optional)

Custom cấu trúc JSON trước khi gửi:

```cpp
std::string my_json_transformer(const std::string& original_json) {
    // Thêm timestamp
    std::time_t now = std::time(nullptr);
    
    // Wrap trong custom structure
    std::string custom_json = "{"
        "\"timestamp\": " + std::to_string(now) + ", "
        "\"device_id\": \"camera_01\", "
        "\"data\": " + original_json + 
    "}";
    
    return custom_json;
}
```

### 3. Tạo Node và Kết nối Pipeline

```cpp
// Tạo MQTT broker node
auto mqtt_broker = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
    "mqtt_broker_0",
    cvedix_nodes::cvedix_broke_for::NORMAL,  // Loại target
    50,   // Cache warning threshold
    200,  // Cache ignore threshold
    my_json_transformer,  // JSON transformer (có thể nullptr)
    my_mqtt_publisher     // MQTT publisher function
);

// Kết nối vào pipeline
tracker->attach_to({detector});
mqtt_broker->attach_to({tracker});
osd->attach_to({mqtt_broker});  // Broker forward dữ liệu
```

### 4. Dynamic Configuration

Có thể thay đổi transformer và publisher sau khi tạo node:

```cpp
// Thay đổi JSON transformer
mqtt_broker->set_json_transformer(new_transformer);

// Thay đổi MQTT publisher
mqtt_broker->set_mqtt_publisher(new_publisher);
```

## Cấu trúc JSON mặc định

JSON được serialize từ `frame_meta` với cấu trúc:

```json
{
  "channel_index": 0,
  "frame_index": 123,
  "width": 1280,
  "height": 720,
  "fps": 30,
  "broke_for": "normal",
  "target_size": 2,
  "targets": [
    {
      "x": 100,
      "y": 200,
      "width": 150,
      "height": 200,
      "primary_class_id": 0,
      "primary_score": 0.95,
      "primary_label": "person",
      "frame_index": 123,
      "channel_index": 0,
      "track_id": 5,
      ...
    }
  ]
}
```

Sau khi qua JSON transformer, cấu trúc có thể được thay đổi theo nhu cầu.

## Ví dụ hoàn chỉnh

Xem file `samples/rknn_rtsp_tracking_mqtt_sample.cpp` để xem ví dụ đầy đủ.

## Lưu ý

1. **MQTT Publisher Function**: User phải tự implement và quản lý MQTT connection
2. **Thread Safety**: MQTT publisher function sẽ được gọi từ broker thread, đảm bảo thread-safe
3. **Error Handling**: Nếu publisher function throw exception, node sẽ log error và tiếp tục
4. **Performance**: Node xử lý bất đồng bộ, không chặn pipeline

## So sánh với Enhanced Broker

- **Enhanced Broker**: Tạo JSON với base64 images, output ra console (debug)
- **MQTT Broker**: Tạo JSON cơ bản, cho phép custom structure, gửi qua MQTT

Có thể dùng cả hai cùng lúc:
- Enhanced broker để debug (xem JSON trong console)
- MQTT broker để gửi dữ liệu lên server

