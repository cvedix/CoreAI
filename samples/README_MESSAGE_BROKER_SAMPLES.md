# Message Broker Samples

## Tổng quan

Các samples này demo cách sử dụng message broker nodes để serialize và gửi dữ liệu detection/recognition results đến external systems.

---

## Sample 1: JSON Console Broker

### File
`message_broker_sample.cpp`

### Mô tả
Serialize face detection results thành JSON và output ra console.

### Pipeline
```
Video → Face Detector → Face Encoder → JSON Console Broker → OSD → Screen
```

### Features
- JSON serialization
- Console output
- Face target data

### Usage
```bash
./build/bin/message_broker_sample
```

### Configuration
```cpp
auto json_broker = std::make_shared<cvedix_json_console_broker_node>(
    "json_broker",
    cvedix_broke_for::FACE  // Broker for face targets
);
```

### Output Format
```json
{
  "channel": 0,
  "frame_id": 123,
  "faces": [
    {
      "id": 1,
      "x": 245,
      "y": 156,
      "width": 89,
      "height": 112,
      "confidence": 0.987,
      "embeddings": [0.123, -0.456, ...]
    }
  ]
}
```

---

## Sample 2: XML Socket Broker

### File
`message_broker_sample2.cpp`

### Mô tả
Serialize vehicle và plate detection results thành XML và gửi qua UDP socket.

### Pipeline
```
Video → Vehicle Detector → Plate Detector → XML Socket Broker → OSD → Screen
```

### Features
- XML serialization
- UDP socket transmission
- Vehicle và plate data

### Usage
```bash
./build/bin/message_broker_sample2
```

### Configuration
```cpp
auto xml_broker = std::make_shared<cvedix_xml_socket_broker_node>(
    "xml_broker",
    "192.168.1.100",  // Destination IP
    6666              // Destination port
);
```

### Output Format
```xml
<frame>
  <channel>0</channel>
  <frame_id>123</frame_id>
  <vehicles>
    <vehicle>
      <id>1</id>
      <x>100</x>
      <y>200</y>
      <width>150</width>
      <height>100</height>
      <plate>ABC-123</plate>
    </vehicle>
  </vehicles>
</frame>
```

---

## Sample 3: MQTT JSON Receiver

### File
`mqtt_json_receiver_sample.cpp`

### Mô tả
Nhận và xử lý JSON messages từ MQTT broker.

### Features
- MQTT subscription
- JSON parsing
- Custom callback processing
- Auto-reconnect

### Usage
```bash
# Default broker
./build/bin/mqtt_json_receiver_sample

# Custom broker
./build/bin/mqtt_json_receiver_sample \
    broker.example.com \
    1883 \
    events \
    username \
    password
```

### Requirements
- Build với `-DCVEDIX_WITH_MQTT=ON`
- libmosquitto-dev installed

### Configuration
```cpp
auto mqtt_receiver = std::make_shared<cvedix_mqtt_json_receiver>(
    "mqtt_receiver",
    broker_url,    // MQTT broker URL
    port,          // MQTT port (default: 1883)
    topic,         // Topic to subscribe
    username,      // Optional username
    password       // Optional password
);

// Set callback
mqtt_receiver->set_message_callback([](const json& msg) {
    // Process JSON message
    std::cout << "Received: " << msg.dump() << std::endl;
});
```

---

## Sample 4: Kafka Message Broker

### File
`message_broker_kafka_sample.cpp`

### Mô tả
Gửi detection results đến Kafka broker.

### Pipeline
```
Video → Detector → Kafka Broker → OSD → Screen
```

### Features
- Kafka producer
- Topic-based messaging
- High-throughput streaming

### Usage
```bash
./build/bin/message_broker_kafka_sample
```

### Requirements
- Build với `-DCVEDIX_WITH_KAFKA=ON`
- Kafka libraries

### Configuration
```cpp
auto kafka_broker = std::make_shared<cvedix_kafka_broker_node>(
    "kafka_broker",
    "kafka_broker:9092",  // Kafka broker address
    "detection_topic"     // Topic name
);
```

---

## So sánh các Broker Types

| Broker | Format | Transport | Use Case |
|--------|--------|-----------|----------|
| **JSON Console** | JSON | Console | Debug, logging |
| **XML Socket** | XML | UDP/TCP | Legacy systems |
| **MQTT** | JSON | MQTT | IoT, real-time |
| **Kafka** | JSON | Kafka | High-throughput |

---

## Build

### Basic Brokers
```bash
cd build
cmake ..
make message_broker_sample
make message_broker_sample2
```

### MQTT Broker
```bash
cmake -DCVEDIX_WITH_MQTT=ON ..
make mqtt_json_receiver_sample
```

### Kafka Broker
```bash
cmake -DCVEDIX_WITH_KAFKA=ON ..
make message_broker_kafka_sample
```

---

## Use Cases

### 1. Debug & Logging
```bash
# Output to console
./build/bin/message_broker_sample
```

### 2. Legacy System Integration
```bash
# Send XML via UDP
./build/bin/message_broker_sample2
```

### 3. IoT Integration
```bash
# MQTT messaging
./build/bin/mqtt_json_receiver_sample
```

### 4. High-Throughput Streaming
```bash
# Kafka streaming
./build/bin/message_broker_kafka_sample
```

---

## Message Format Examples

### Face Detection JSON
```json
{
  "timestamp": "2024-01-01T12:00:00Z",
  "channel": 0,
  "frame_id": 1234,
  "faces": [
    {
      "track_id": 1,
      "bbox": {"x": 100, "y": 200, "w": 150, "h": 180},
      "confidence": 0.95,
      "embeddings": [0.1, -0.2, 0.3, ...],
      "landmarks": [
        {"x": 120, "y": 250},
        {"x": 180, "y": 250},
        ...
      ]
    }
  ]
}
```

### Vehicle Detection XML
```xml
<detection>
  <timestamp>2024-01-01T12:00:00Z</timestamp>
  <channel>0</channel>
  <vehicles>
    <vehicle>
      <track_id>5</track_id>
      <bbox x="300" y="400" w="200" h="150"/>
      <class>car</class>
      <confidence>0.92</confidence>
      <plate>ABC-123</plate>
    </vehicle>
  </vehicles>
</detection>
```

---

## Customization

### Custom JSON Format
```cpp
auto custom_broker = std::make_shared<cvedix_json_console_broker_node>(
    "broker",
    cvedix_broke_for::FACE
);

// Custom serialization (if supported by node)
// Modify node implementation for custom format
```

### Filter Specific Data
```cpp
// Only send high-confidence detections
// (Implementation depends on node API)
```

### Batch Messages
```cpp
// Send multiple frames in one message
// (Implementation depends on node API)
```

---

## Performance

### Console Output
- **Latency**: < 1ms
- **Throughput**: Unlimited (console buffer)

### Socket Transmission
- **Latency**: 1-5ms (local network)
- **Throughput**: ~1000 messages/sec

### MQTT
- **Latency**: 10-50ms (depends on broker)
- **Throughput**: ~100-500 messages/sec

### Kafka
- **Latency**: 5-20ms
- **Throughput**: ~10,000+ messages/sec

---

## Troubleshooting

### Issue: Socket connection failed
- Check destination IP/port
- Verify firewall rules
- Test network connectivity

### Issue: MQTT connection failed
- Verify broker URL/port
- Check authentication
- Test with MQTT client tool

### Issue: Kafka producer failed
- Check broker address
- Verify topic exists
- Check Kafka cluster status

---

## Related Documentation

- [MQTT JSON Transformer](README_MQTT_JSON_TRANSFORMER.md) - MQTT details
- [Main README](README.md) - Tổng quan samples

