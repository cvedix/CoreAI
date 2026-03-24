# cvedix_osd_node — Unified OSD Rendering

Single OSD node with **18 configurable rendering layers**. Auto-detects data in `cvedix_frame_meta` and renders accordingly.

## Quick Start

```cpp
#include "cvedix/nodes/osd/cvedix_osd_node.h"

// Basic usage (default config: green dots, track IDs, trails)
auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

// With Chinese/Unicode font support
auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd", "./fonts/simhei.ttf");
```

## Configuration

All layers are toggled via `unified_osd_config`:

```cpp
cvedix_nodes::unified_osd_config cfg;

// ── Target rendering ──
cfg.show_bbox          = true;   // bounding box (default: false)
cfg.show_label         = true;   // class label text (default: false)
cfg.show_track_id      = true;   // tracking ID (default: true)
cfg.show_track_trail   = true;   // tracking path (default: true)
cfg.show_center_dot    = true;   // center point dot (default: true)
cfg.show_sub_targets   = false;  // sub-target bboxes (default: false)

// ── Style ──
cfg.dot_color       = {0, 255, 0};     // green
cfg.trail_color     = {0, 255, 0};     // green
cfg.bbox_color      = {255, 255, 0};   // cyan
cfg.alert_color     = {0, 0, 255};     // red (crossed/alarm)
cfg.bbox_thickness  = 2;
cfg.label_font_scale = 0.4;
cfg.dot_radius      = 5;

// ── BA layers ──
cfg.enable_ba_crossline   = true;
cfg.enable_ba_crowding    = true;
cfg.enable_ba_jam         = true;
cfg.enable_ba_stop        = true;
cfg.enable_ba_enter_exit  = true;

// ── Domain layers ──
cfg.enable_face           = true;
cfg.enable_pose           = true;
cfg.enable_instance_mask  = true;
cfg.enable_text_region    = true;
cfg.enable_expr           = true;
cfg.enable_lane           = true;
cfg.enable_plate          = true;
cfg.enable_seg            = true;
cfg.enable_mllm           = true;
cfg.enable_sub_thumbnails = true;

// ── Static geometry ──
cfg.show_static_lines  = true;
cfg.show_static_zones  = true;

osd->update_config(cfg);
```

---

## Pipeline Pattern

All pipelines follow the same pattern:

```
Source → Detector → Tracker → [BA Node] → OSD → Destination
```

The OSD node reads `frame_meta->ba_results` and auto-renders based on `cvedix_ba_type`.

---

## BA Event Usage Examples

### 1. Crossline Counting

Triggers when tracked objects cross a defined line.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_line_crossline_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

// Define crosslines per channel
cvedix_objects::cvedix_point start(0, 250);
cvedix_objects::cvedix_point end(700, 220);
std::map<int, cvedix_objects::cvedix_line> lines = {
    {0, cvedix_objects::cvedix_line(start, end)}
};

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_line_crossline_node>("ba_crossline", lines);
auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

tracker->attach_to({detector});
ba->attach_to({tracker});
osd->attach_to({ba});
screen->attach_to({osd});
```

**OSD renders:** crossline with direction arrows, per-line counts, total count, crossed targets in red.

**Config toggle:** `cfg.enable_ba_crossline`

---

### 2. Crowding Detection

Triggers when object count inside ROI ≥ threshold for N seconds.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_area_crowding_node.h"

// ROI polygon per channel
std::map<int, std::vector<cvedix_objects::cvedix_point>> rois = {
    {0, { {20,360}, {400,250}, {700,250}, {700,700}, {30,700} }}
};

// Config: threshold=3, alarm_seconds=2.0
std::map<int, cvedix_nodes::crowding_config> configs = {
    {0, cvedix_nodes::crowding_config(3, 2.0, "Lobby Area")}
};

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_area_crowding_node>(
    "ba_crowding", rois, configs, 30, false, false);
auto osd = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd");

ba->attach_to({tracker});
osd->attach_to({ba});
```

**OSD renders:** ROI polygon in red, "CROWDING" alert banner (top-right), object count, involved targets highlighted.

**Config toggle:** `cfg.enable_ba_crowding`

---

### 3. Area Enter/Exit

Triggers when objects enter or exit a defined polygon area.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_area_enter_exit_node.h"

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_area_enter_exit_node>(
    "ba_enter_exit", rois);

ba->attach_to({tracker});
osd->attach_to({ba});
```

**OSD renders:** polygon flash (green=enter, red=exit), TTL-based alerts ("ID 42 - Enter"), fading after 50 frames.

**Config toggle:** `cfg.enable_ba_enter_exit`

---

### 4. Traffic Jam Detection

Triggers when tracked objects inside ROI are stationary for configured duration.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_area_jam_node.h"

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_area_jam_node>(
    "ba_jam", rois);

ba->attach_to({tracker});
osd->attach_to({ba});
```

**OSD renders:** ROI polygon in red (when JAM active), involved target bboxes in red. Clears on UNJAM event.

**Config toggle:** `cfg.enable_ba_jam`

---

### 5. Stop Detection

Triggers when individual objects stop moving inside a defined area.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_stop_node.h"

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_stop_node>(
    "ba_stop", rois);

ba->attach_to({tracker});
osd->attach_to({ba});
```

**OSD renders:** ROI polygon in green, stopped targets highlighted with red bbox. Clears on UNSTOP.

**Config toggle:** `cfg.enable_ba_stop`

---

### 6. Intrusion Detection

Triggers when objects enter a restricted zone.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_intrusion_detection_node.h"

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_intrusion_detection_node>(
    "ba_intrusion", rois);

ba->attach_to({tracker});
osd->attach_to({ba});
```

**BA events:** `INTRUSION_START` / `INTRUSION_END`

---

### 7. Loitering Detection

Triggers when objects stay in an area longer than configured time.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_area_loitering_node.h"

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_area_loitering_node>(
    "ba_loitering", rois);

ba->attach_to({tracker});
osd->attach_to({ba});
```

**BA events:** `LOITERING` / `LOITERING_END`

---

### 8. Speed Estimation

Estimates speed of objects crossing between two lines.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_line_speed_estimation_node.h"

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_line_speed_estimation_node>(
    "ba_speed", lines);

ba->attach_to({tracker});
osd->attach_to({ba});
```

**BA events:** `SPEED`

---

### 9. Wrong-Way Detection

Triggers when objects move against designated direction.

```cpp
#include "cvedix/nodes/ba/cvedix_ba_line_wrong_way_node.h"

auto ba = std::make_shared<cvedix_nodes::cvedix_ba_line_wrong_way_node>(
    "ba_wrong_way", lines);

ba->attach_to({tracker});
osd->attach_to({ba});
```

**BA events:** `WRONG_WAY`

---

## All BA Event Types

| Event Type | BA Node | OSD Layer |
|---|---|---|
| `CROSSLINE` | `cvedix_ba_line_crossline_node` | `enable_ba_crossline` |
| `CROWDING` | `cvedix_ba_area_crowding_node` | `enable_ba_crowding` |
| `JAM` / `UNJAM` | `cvedix_ba_area_jam_node` | `enable_ba_jam` |
| `STOP` / `UNSTOP` | `cvedix_ba_stop_node` | `enable_ba_stop` |
| `AREA_ENTER` / `AREA_EXIT` | `cvedix_ba_area_enter_exit_node` | `enable_ba_enter_exit` |
| `INTRUSION_START` / `INTRUSION_END` | `cvedix_ba_intrusion_detection_node` | — |
| `LOITERING` / `LOITERING_END` | `cvedix_ba_area_loitering_node` | — |
| `SPEED` | `cvedix_ba_line_speed_estimation_node` | — |
| `DIRECTION` | `cvedix_ba_line_direction_violation_node` | — |
| `WRONG_WAY` | `cvedix_ba_line_wrong_way_node` | — |
| `DWELL` | `cvedix_ba_area_dwell_time_node` | — |
| `QUEUE` | `cvedix_ba_area_queue_length_node` | — |
| `FALL` | `cvedix_ba_fall_detection_node` | — |
| `FIGHT` | `cvedix_ba_fight_detection_node` | — |
| `PARKING` | `cvedix_ba_area_parking_violation_node` | — |
| `RED_LIGHT` | `cvedix_ba_line_red_light_violation_node` | — |
| `ILLEGAL_TURN` | `cvedix_ba_area_illegal_turn_node` | — |
| `LANE_VIOLATION` | `cvedix_ba_area_lane_violation_node` | — |
| `NO_ENTRY` | `cvedix_ba_area_no_entry_zone_node` | — |
| `ILLEGAL_UTURN` | `cvedix_ba_line_illegal_uturn_node` | — |
| `HELMET` | `cvedix_ba_area_helmet_violation_node` | — |
| `STOP_LINE` | `cvedix_ba_movement_node` | — |

> **Note:** BA events marked with "—" for OSD Layer don't have dedicated rendering. They emit `cvedix_ba_result` events that can be consumed via MQTT or custom handlers. The basic target rendering (dots, trails, bboxes) still applies.

---

## Domain Rendering Layers

These layers auto-detect from `cvedix_frame_meta` fields:

| Layer | Detects from | Toggle |
|---|---|---|
| Face | `meta->face_targets` | `enable_face` |
| Pose | `meta->pose_targets` | `enable_pose` |
| Instance Mask | `target->mask` | `enable_instance_mask` |
| Text Region | `meta->text_targets` | `enable_text_region` |
| Expression | `text_target->flags` | `enable_expr` |
| Lane Mask | `meta->mask` | `enable_lane` |
| Plate | `target->primary_label` (color_text) | `enable_plate` |
| Segmentation | `meta->mask` | `enable_seg` |
| MLLM Description | `meta->description` | `enable_mllm` |
| Sub Thumbnails | `target->sub_targets` | `enable_sub_thumbnails` |

---

## Static Geometry

Draw always-on lines and zones:

```cpp
// Static lines (always visible, no BA logic)
std::vector<cvedix_nodes::unified_static_line_config> lines = {
    {{cvedix_objects::cvedix_line({100,200}, {500,200})}, {0,255,0}, "Entry Line"}
};
osd->set_static_lines(lines);

// Static zones (semi-transparent polygon fill)
std::vector<cvedix_nodes::unified_static_zone_config> zones = {
    {{ {{50,50}, {300,50}, {300,300}, {50,300}} }, {0,200,0}, "Zone A"}
};
osd->set_static_zones(zones);
```

---

## Public API

| Method | Description |
|---|---|
| `update_config(cfg)` | Update all rendering toggles at runtime |
| `get_config()` | Get current config |
| `set_static_lines(lines)` | Set always-on line overlays |
| `set_static_zones(zones)` | Set always-on zone overlays |
| `set_line_configs(ch, configs)` | Set crossline display configs per channel |
| `set_line_color(ch, idx, color)` | Change a specific crossline color |
| `set_seg_config(classes, colors)` | Set segmentation class labels and colors |
