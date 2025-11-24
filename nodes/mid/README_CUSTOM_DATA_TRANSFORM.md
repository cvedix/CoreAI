# Custom Data Transform Node

## Tổng quan

`cvedix_custom_data_transform_node` là một node cho phép tùy chỉnh dữ liệu `frame_meta` theo yêu cầu khách hàng trước khi gửi đến các node tiếp theo (như broker).

## Tính năng

- ✅ Filter targets theo điều kiện (score, class_id, track_id, ...)
- ✅ Thêm/sửa/xóa thông tin trong targets
- ✅ Modify frame data
- ✅ Drop frames không cần thiết (return nullptr)
- ✅ Flexible với callback function

## Pipeline Flow

```
Previous Node → Custom Transform → Next Node (Broker/OSD/...)
```

## Cách sử dụng

### 1. Basic Usage - Filter targets theo score

```cpp
#include "cvedix/nodes/mid/cvedix_custom_data_transform_node.h"

auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (!meta || meta->targets.empty()) {
            return meta;
        }
        
        // Filter targets với score >= 0.5
        auto it = meta->targets.begin();
        while (it != meta->targets.end()) {
            if ((*it)->primary_score < 0.5f) {
                it = meta->targets.erase(it);
            } else {
                ++it;
            }
        }
        
        return meta;
    }
);

// Pipeline: tracker → transform → broker
custom_transform->attach_to({tracker});
broker->attach_to({custom_transform});
```

### 2. Filter targets đã được track

```cpp
auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (!meta || meta->targets.empty()) {
            return meta;
        }
        
        // Chỉ giữ targets đã được track (track_id >= 0)
        auto it = meta->targets.begin();
        while (it != meta->targets.end()) {
            if ((*it)->track_id < 0) {  // track_id = -1 means not tracked
                it = meta->targets.erase(it);
            } else {
                ++it;
            }
        }
        
        return meta;
    }
);
```

### 3. Filter theo class_id

```cpp
auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (!meta || meta->targets.empty()) {
            return meta;
        }
        
        // Chỉ giữ targets là person (class_id = 0)
        auto it = meta->targets.begin();
        while (it != meta->targets.end()) {
            if ((*it)->primary_class_id != 0) {
                it = meta->targets.erase(it);
            } else {
                ++it;
            }
        }
        
        return meta;
    }
);
```

### 4. Thêm custom label

```cpp
auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (!meta || meta->targets.empty()) {
            return meta;
        }
        
        // Thêm custom label cho targets có score cao
        for (auto& target : meta->targets) {
            if (target->primary_score > 0.7f) {
                target->primary_label = "HighConfidence_" + target->primary_label;
            }
        }
        
        return meta;
    }
);
```

### 5. Drop frames không có targets

```cpp
auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (!meta || meta->targets.empty()) {
            // Drop frame nếu không có targets
            return nullptr;
        }
        
        return meta;
    }
);
```

### 6. Complex transformation - Multiple conditions

```cpp
auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (!meta || meta->targets.empty()) {
            return meta;
        }
        
        // Filter với nhiều điều kiện:
        // - Score >= 0.3
        // - Đã được track (track_id >= 0)
        // - Là person (class_id = 0)
        auto it = meta->targets.begin();
        while (it != meta->targets.end()) {
            auto& target = *it;
            bool should_keep = 
                target->primary_score >= 0.3f &&
                target->track_id >= 0 &&
                target->primary_class_id == 0;
            
            if (!should_keep) {
                it = meta->targets.erase(it);
            } else {
                // Thêm custom metadata
                target->primary_label = "TrackedPerson";
                ++it;
            }
        }
        
        // Drop frame nếu không còn targets sau khi filter
        if (meta->targets.empty()) {
            return nullptr;
        }
        
        return meta;
    }
);
```

### 7. Dynamic transform function

```cpp
// Có thể thay đổi transform function sau khi tạo node
auto custom_transform = std::make_shared<cvedix_nodes::cvedix_custom_data_transform_node>(
    "custom_transform_0",
    nullptr  // Không set transform function ban đầu
);

// Set transform function sau
custom_transform->set_transform_func(
    [](std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        // Your custom logic here
        return meta;
    }
);
```

## API Reference

### Constructor

```cpp
cvedix_custom_data_transform_node(
    std::string node_name,
    std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> transform_func = nullptr
);
```

### Methods

```cpp
// Set transform function
void set_transform_func(
    std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> func
);

// Get current transform function
std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> get_transform_func() const;
```

## Lưu ý

1. **Performance**: Transform function được gọi trong handle thread, nên cần tối ưu để không block pipeline
2. **Thread Safety**: Transform function nên chỉ modify frame_meta, không nên access shared resources không thread-safe
3. **Return nullptr**: Nếu return `nullptr`, frame sẽ bị drop và không được forward đến next nodes
4. **Error Handling**: Nếu transform function throw exception, original frame sẽ được forward

## Ví dụ trong Sample

Xem file `samples/rknn_rtsp_tracking_mqtt_sample.cpp` để xem ví dụ sử dụng trong pipeline thực tế.

