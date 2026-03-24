# BA Node Event Export Format

This document describes the JSON event format exported by Behavior Analysis (BA) nodes through broker nodes (MQTT, WebSocket, etc.).

## Common Fields

Every BA event contains these fields:

| Field | Type | Description |
|-------|------|-------------|
| `event_id` | `string` (UUID) | Unique identifier for this event |
| `event_timestamp_ms` | `number` | Event timestamp (relative to stream start, ms) |
| `system_timestamp` | `number` | System clock epoch timestamp (ms) |
| `system_datetime` | `string` | ISO 8601 datetime (e.g., `"2026-03-23T14:47:06Z"`) |
| `region_type` | `string` | `"area"` or `"line"` |
| `region_id` | `string` (UUID) | UUID of the configured area/line |
| `region_name` | `string` | User-defined name for the region |
| `region_index` | `number` | Index of the region in configuration |

## Target Fields

Target information can appear at top-level (single-target events) or inside a `targets[]` array (group events).

| Field | Type | Description |
|-------|------|-------------|
| `location.x` | `number` | Normalized X (0.0–1.0 relative to frame width) |
| `location.y` | `number` | Normalized Y (0.0–1.0 relative to frame height) |
| `location.width` | `number` | Normalized width |
| `location.height` | `number` | Normalized height |
| `object_class` | `string` | Detection class (e.g., `"Person"`, `"Vehicle"`) |
| `ref_tracking_id` | `string` (UUID) | Unique tracking reference ID |
| `crop_image` | `string` (base64) | Cropped target image (**optional**, requires `include_target_crops = true`) |

---

## 1. Crowding Detection

Detects when the number of objects in an area exceeds a threshold.

**BA Type:** `CROWDING`  
**Event:** Group event — multiple targets aggregated

### Event Schema

```json
{
  "event_id": "d41de3b2-3682-4c6d-96c3-6b506d5d6c7b",
  "event_timestamp_ms": 92905,
  "system_timestamp": 1774277226397,
  "system_datetime": "2026-03-23T14:47:06Z",
  "region_type": "area",
  "region_id": "771c94d7-4589-43cf-9b9e-220682a7bfa1",
  "region_name": "Crowding area 1",
  "region_index": 0,
  "targets": [
    {
      "location": {
        "x": 0.5206,
        "y": 0.2108,
        "width": 0.0159,
        "height": 0.0243
      },
      "object_class": "Vehicle",
      "ref_tracking_id": "a4ccc1b2-8350-4bf1-b676-2805e670523d"
    },
    {
      "location": {
        "x": 0.4999,
        "y": 0.2233,
        "width": 0.0217,
        "height": 0.0312
      },
      "object_class": "Vehicle",
      "ref_tracking_id": "e355753e-0af7-48fa-9a14-73d0163d154e"
    }
  ]
}
```

### Configuration

```cpp
crowding_config config;
config.name = "Crowding area 1";
config.id = "771c94d7-...";          // UUID from external config
config.obj_count_threshold = 3;
config.alarm_seconds = 5;

auto node = std::make_shared<cvedix_ba_area_crowding_node>(
    "crowding", rois, configs, 30,
    true,   // need_record_image
    false,  // need_record_video
    true    // include_target_crops ← enables crop_image in output
);
```

---

## 2. Intrusion Detection (Area Enter/Exit)

Detects unauthorized access to restricted areas. Emits **Start** event on entry and **End** event on exit.

**BA Types:** `AREA_ENTER` (start), `AREA_EXIT` (end)  
**Event:** Single-target — one event per object

### Start Event (Intrusion Begin)

```json
{
  "event_id": "d7353de9-6ce8-4bea-b50f-49fceefce50b",
  "event_timestamp_ms": 527237,
  "system_timestamp": 1774277916321,
  "system_datetime": "2026-03-23T14:58:36Z",
  "region_type": "area",
  "region_id": "84f7990e-238f-4037-ba20-c7fb8c07be36",
  "region_name": "Intrusion Detection Area 1",
  "region_index": 0,
  "location": {
    "x": 0.5032,
    "y": 0.2446,
    "width": 0.0244,
    "height": 0.0387
  },
  "object_class": "Vehicle",
  "ref_tracking_id": "fea85200-ea2c-420a-80cf-3646072c3a4d"
}
```

### End Event (Intrusion End)

```json
{
  "event_id": "f94ad664-8432-44cf-a153-f01a18211744",
  "event_timestamp_ms": 447237,
  "event_duration_ms": 7400,
  "system_timestamp": 1774277836312,
  "system_datetime": "2026-03-23T14:57:16Z",
  "region_type": "area",
  "region_id": "84f7990e-238f-4037-ba20-c7fb8c07be36",
  "region_name": "Intrusion Detection Area 1",
  "region_index": 0,
  "location": {
    "x": 0.5201,
    "y": 0.2040,
    "width": 0.0228,
    "height": 0.0345
  },
  "object_class": "Vehicle",
  "ref_tracking_id": "8aa97a4c-baef-4a0f-8158-cfa80148ead8"
}
```

> **`event_duration_ms`**: Automatically computed as the time difference between enter and exit timestamps.

### Configuration

```cpp
area_alert_config config;
config.name = "Intrusion Detection Area 1";
config.id = "84f7990e-...";         // UUID from external config
config.alert_on_enter = true;
config.alert_on_exit = true;

auto node = std::make_shared<cvedix_ba_area_enter_exit_node>(
    "intrusion", areas, configs,
    true,   // need_record_image
    false   // need_record_video
);
node->include_target_crops = true;   // enables crop_image in output
```

---

## 3. Crossline Detection

Detects when an object crosses a defined line.

**BA Type:** `CROSSLINE`  
**Event:** Single-target — one event per crossing

### Event Schema

```json
{
  "event_id": "b2c4e6f8-1a3b-5c7d-9e0f-2a4b6c8d0e1f",
  "event_timestamp_ms": 152300,
  "system_timestamp": 1774277500123,
  "system_datetime": "2026-03-23T14:51:40Z",
  "region_type": "line",
  "region_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "region_name": "Main Gate Line",
  "region_index": 0,
  "location": {
    "x": 0.4521,
    "y": 0.3102,
    "width": 0.0315,
    "height": 0.0487
  },
  "object_class": "Person",
  "ref_tracking_id": "c5d6e7f8-9a0b-1c2d-3e4f-5a6b7c8d9e0f"
}
```

### Configuration

```cpp
crossline_config config;
config.name = "Main Gate Line";

auto node = std::make_shared<cvedix_ba_line_crossline_node>(
    "crossline", lines, configs,
    true,   // need_record_image
    false,  // need_record_video
    true    // include_target_crops
);
```

---

## Crop Image (Optional)

When `include_target_crops = true`, each target includes a base64-encoded cropped image:

```json
{
  "location": { ... },
  "object_class": "Person",
  "ref_tracking_id": "...",
  "crop_image": "/9j/4AAQSkZJRgABAQ..."
}
```

**Default:** `false` (disabled to save bandwidth)  
**Enable per node:** Set `include_target_crops = true` on the node instance or via constructor.

---

## Broker-Level Fields

These fields are added by the broker node during serialization, NOT by `cvedix_ba_result`:

| Field | Type | Description |
|-------|------|-------------|
| `$id` | `string` | Schema identifier (e.g., `"event-intrusion"`, `"event-crowd-detection"`) |
| `$version` | `number` | Schema version |
| `instance_id` | `string` (UUID) | Analytics pipeline instance ID |

---

## `cvedix_ba_result` Field Reference

| `cvedix_ba_result` field | JSON output field | Auto-populated by |
|---|---|---|
| `event_id` | `event_id` | `stamp_now()` |
| `event_timestamp_ms` | `event_timestamp_ms` | `stamp_now()` |
| `system_timestamp` | `system_timestamp` | `stamp_now()` |
| `system_datetime` | `system_datetime` | `stamp_now()` |
| `event_duration_ms` | `event_duration_ms` | BA node (enter_exit only) |
| `region_type` | `region_type` | BA node |
| `region_id` | `region_id` / `area_id` | BA node (from config.id) |
| `region_name` | `region_name` / `area_name` | BA node (from config.name) |
| `region_index` | `region_index` | BA node |
| `involve_target_details[].location_x/y/w/h` | `location` | `populate_target_details()` |
| `involve_target_details[].object_class` | `object_class` | `populate_target_details()` |
| `involve_target_details[].ref_tracking_id` | `ref_tracking_id` | `populate_target_details()` |
| `involve_target_details[].crop` | `crop_image` | `populate_target_details()` (when enabled) |
