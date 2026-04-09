# CVEDIX MQTT Client

## Tổng quan

`cvedix_mqtt_client` là một wrapper class cho thư viện libmosquitto, cung cấp:
- Kết nối tự động và tự động reconnect
- Thread-safe publishing
- Monitoring trạng thái kết nối
- Callbacks cho các sự kiện kết nối/publish

## Yêu cầu

- libmosquitto-dev (cài đặt: `sudo apt-get install libmosquitto-dev`)

## Cách sử dụng

### 1. Khởi tạo MQTT Client

```cpp
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"

// Tạo client
auto mqtt_client = std::make_unique<cvedix_utils::cvedix_mqtt_client>(
    "anhoidong.datacenter.cvedix.com",  // Broker URL
    1883,                                // Port
    "client_id_123",                     // Client ID (optional, auto-generated if empty)
    60                                   // Keepalive (seconds)
);
```

### 2. Thiết lập Callbacks (Optional)

```cpp
// Callback khi kết nối
mqtt_client->set_on_connect_callback([](bool success) {
    if (success) {
        std::cout << "Connected to MQTT broker" << std::endl;
    } else {
        std::cerr << "Connection failed" << std::endl;
    }
});

// Callback khi disconnect
mqtt_client->set_on_disconnect_callback([]() {
    std::cout << "Disconnected from MQTT broker" << std::endl;
});

// Callback khi publish thành công
mqtt_client->set_on_publish_callback([](int mid) {
    std::cout << "Message published, ID: " << mid << std::endl;
});
```

### 3. Kết nối đến Broker

```cpp
// Kết nối không cần authentication
if (mqtt_client->connect()) {
    std::cout << "Connection initiated" << std::endl;
}

// Hoặc với username/password
if (mqtt_client->connect("username", "password")) {
    std::cout << "Connection initiated with authentication" << std::endl;
}
```

### 4. Bật Auto-Reconnect

```cpp
// Bật auto-reconnect với interval 5 giây
mqtt_client->set_auto_reconnect(true, 5000);
```

### 5. Publish Messages

```cpp
// Kiểm tra kết nối
if (mqtt_client->is_ready()) {
    // Publish với QoS 1, không retain
    int mid = mqtt_client->publish("events", json_message, 1, false);
    
    if (mid >= 0) {
        std::cout << "Message published successfully, ID: " << mid << std::endl;
    } else {
        std::cerr << "Publish failed: " << mqtt_client->get_last_error() << std::endl;
    }
}
```

### 6. Disconnect và Cleanup

```cpp
// Disconnect
mqtt_client->disconnect();

// Hoặc để destructor tự động cleanup
mqtt_client.reset();
```

## Ví dụ đầy đủ với MQTT Broker Node

```cpp
#include "cvedix/nodes/broker/cvedix_mqtt_broker_node.h"
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"

// Global MQTT client
static std::unique_ptr<cvedix_utils::cvedix_mqtt_client> g_mqtt_client = nullptr;

// MQTT publisher function
void mqtt_publisher(const std::string& json_message) {
    if (g_mqtt_client && g_mqtt_client->is_ready()) {
        int mid = g_mqtt_client->publish("events", json_message, 1, false);
        if (mid < 0) {
            std::cerr << "Publish failed: " << g_mqtt_client->get_last_error() << std::endl;
        }
    }
}

int main() {
    // Initialize MQTT client
    g_mqtt_client = std::make_unique<cvedix_utils::cvedix_mqtt_client>(
        "anhoidong.datacenter.cvedix.com", 1883
    );
    g_mqtt_client->set_auto_reconnect(true, 5000);
    g_mqtt_client->connect();
    
    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Create MQTT broker node
    auto mqtt_broker = std::make_shared<cvedix_nodes::cvedix_mqtt_broker_node>(
        "mqtt_broker_0",
        cvedix_nodes::cvedix_broke_for::NORMAL,
        50, 200,
        nullptr,              // No JSON transformer
        mqtt_publisher        // MQTT publisher function
    );
    
    // ... setup pipeline ...
    
    // Cleanup
    g_mqtt_client->disconnect();
    g_mqtt_client.reset();
    
    return 0;
}
```

## API Reference

### Constructor

```cpp
cvedix_mqtt_client(
    const std::string& broker_url,
    int port = 1883,
    const std::string& client_id = "",
    int keepalive = 60
);
```

### Methods

- `bool connect(const std::string& username = "", const std::string& password = "")` - Kết nối đến broker
- `void disconnect()` - Ngắt kết nối
- `int publish(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false)` - Publish message
- `bool is_connected() const` - Kiểm tra đã kết nối chưa
- `bool is_ready() const` - Kiểm tra sẵn sàng publish
- `void set_auto_reconnect(bool enable, int reconnect_interval_ms = 5000)` - Bật/tắt auto-reconnect
- `std::string get_last_error() const` - Lấy lỗi cuối cùng

## Lưu ý

1. **Thread Safety**: Class này thread-safe cho việc publish
2. **Auto-Reconnect**: Mặc định bật auto-reconnect, tự động reconnect khi mất kết nối
3. **Async Connection**: Kết nối là async, cần đợi một chút sau khi gọi `connect()`
4. **Error Handling**: Luôn kiểm tra `is_ready()` trước khi publish
5. **Cleanup**: Destructor tự động cleanup, nhưng nên gọi `disconnect()` trước khi destroy

