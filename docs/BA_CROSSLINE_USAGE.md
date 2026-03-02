# BA Crossline Node - SDK Usage Guide

## Overview

`cvedix_ba_crossline_node` is a behavior analysis node that detects when tracked objects cross user-defined lines. Key features:

- **Multi-line per channel** - Multiple detection lines for each video channel
- **Direction detection** - Distinguishes between IN and OUT crossings
- **Direction filtering** - Only trigger events for specific directions
- **Custom colors** - Each line can have its own color
- **Runtime updates** - Add/remove/modify lines dynamically
- **Per-line counters** - Track crossings for each line independently

---

## Quick Start

```cpp
#include "cvedix/nodes/ba/cvedix_ba_crossline_node.h"
#include "cvedix/nodes/osd/cvedix_ba_crossline_osd_node.h"

// 1. Define lines for channel 0
std::map<int, std::vector<cvedix_objects::cvedix_line>> lines = {
    {0, {
        cvedix_objects::cvedix_line(
            cvedix_objects::cvedix_point(0, 300),    // start
            cvedix_objects::cvedix_point(700, 300)   // end
        )
    }}
};

// 2. Create crossline node
auto ba_crossline = std::make_shared<cvedix_nodes::cvedix_ba_crossline_node>(
    "crossline",
    lines,
    true,   // record image on crossing
    false   // don't record video
);

// 3. Create OSD for visualization
auto osd = std::make_shared<cvedix_nodes::cvedix_ba_crossline_osd_node>("osd");

// 4. Attach to pipeline (after tracker)
ba_crossline->attach_to({tracker_node});
osd->attach_to({ba_crossline});
```

---

## Advanced: Multi-Line with Color and Direction

```cpp
using namespace cvedix_nodes;
using namespace cvedix_objects;

// Create entrance line (only count IN direction) - Red color
crossline_config entrance(
    cvedix_line(cvedix_point(0, 200), cvedix_point(700, 200)),
    cv::Scalar(0, 0, 255),      // Red (BGR)
    "entrance",                  // Name
    cvedix_ba_direct_type::IN   // Only count IN direction
);

// Create exit line (only count OUT direction) - Blue color
crossline_config exit_line(
    cvedix_line(cvedix_point(0, 400), cvedix_point(700, 400)),
    cv::Scalar(255, 0, 0),      // Blue (BGR)
    "exit",
    cvedix_ba_direct_type::OUT  // Only count OUT direction
);

// Create gate line (count both directions) - Green color
crossline_config gate(
    cvedix_line(cvedix_point(0, 300), cvedix_point(700, 300)),
    cv::Scalar(0, 255, 0),      // Green (BGR)
    "gate",
    cvedix_ba_direct_type::BOTH // Count both directions (default)
);

// Create node and add lines with configurations
auto ba_crossline = std::make_shared<cvedix_ba_crossline_node>("crossline", 
    std::map<int, std::vector<cvedix_line>>(), false, false);

ba_crossline->add_line(0, entrance);
ba_crossline->add_line(0, exit_line);
ba_crossline->add_line(0, gate);
```

---

## Runtime APIs

### Add/Remove Lines

```cpp
// Add a new line at runtime, returns line index
int idx = ba_crossline->add_line(0, cvedix_line(Point(100,350), Point(600,350)));

// Add line with full configuration
int idx2 = ba_crossline->add_line(0, crossline_config(line, color, "name", direction));

// Remove specific line by index
ba_crossline->remove_line(0, idx);

// Remove all lines from a channel
ba_crossline->remove_channel_lines(0);

// Clear all lines from all channels
ba_crossline->clear_lines();
```

### Update Color

```cpp
// Change line color at runtime
ba_crossline->set_line_color(0, 0, cv::Scalar(0, 255, 255));  // Yellow
```

### Query Line Status

```cpp
// Get number of lines for channel
size_t count = ba_crossline->get_line_count(0);

// Get crossing count for specific line
int crossings = ba_crossline->get_crossline_count(0, 0);  // channel 0, line 0

// Get line configuration
crossline_config config = ba_crossline->get_line_config(0, 0);

// Get all configs for a channel
std::vector<crossline_config> configs = ba_crossline->get_all_configs(0);
```

---

## Direction Types

| Direction | Enum Value | Behavior |
|-----------|------------|----------|
| **IN** | `cvedix_ba_direct_type::IN` | Only trigger on IN crossings |
| **OUT** | `cvedix_ba_direct_type::OUT` | Only trigger on OUT crossings |
| **BOTH** | `cvedix_ba_direct_type::BOTH` | Trigger on both directions (default) |

---

## BA Result Events

When a crossing is detected, `cvedix_ba_result` is added to `meta->ba_results`:

```cpp
// In a custom node attached after crossline
for (auto& result : meta->ba_results) {
    if (result->type == cvedix_ba_type::CROSSLINE) {
        // result->ba_label: "cross line 0 [IN]" or "cross line 1 [OUT]"
        // result->involve_region_in_frame: line endpoints
        // result->involve_targets: track_ids of objects that crossed
    }
}
```

---

## Complete Pipeline Example

```cpp
int main() {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    // Source
    auto file_src = std::make_shared<cvedix_file_src_node>(
        "file_src", 0, "./video.mp4", 0.4);

    // Detector
    auto detector = std::make_shared<cvedix_yolo_detector_node>(
        "detector", "model.weights", "model.cfg", "classes.txt");

    // Tracker (required for crossline)
    auto tracker = std::make_shared<cvedix_bytetrack_node>(
        "tracker", cvedix_track_for::NORMAL, 0.5, 0.9, 0.6, 20, 15);

    // Multi-line crossline with colors
    std::map<int, std::vector<cvedix_line>> lines = {
        {0, {
            cvedix_line(Point(0, 200), Point(700, 200)),
            cvedix_line(Point(0, 350), Point(700, 350))
        }}
    };
    auto crossline = std::make_shared<cvedix_ba_crossline_node>("crossline", lines);

    // OSD visualization
    auto osd = std::make_shared<cvedix_ba_crossline_osd_node>("osd");

    // Screen output
    auto screen = std::make_shared<cvedix_screen_des_node>("screen", 0);

    // Build pipeline
    detector->attach_to({file_src});
    tracker->attach_to({detector});
    crossline->attach_to({tracker});
    osd->attach_to({crossline});
    screen->attach_to({osd});

    file_src->start();

    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    return 0;
}
```

---

## OSD Visualization Features

The `cvedix_ba_crossline_osd_node` displays:

- **Colored lines** - Each line drawn with its configured color
- **Direction arrows** - Perpendicular arrows showing IN/OUT/BOTH
- **Line labels** - Name at midpoint of each line
- **Per-line counts** - Individual crossing counts on screen
- **Total count** - Aggregate of all crossings

### Set Line Color in OSD

```cpp
// Set line configurations manually
osd->set_line_configs(0, configs);

// Update single line color
osd->set_line_color(0, 0, cv::Scalar(255, 0, 255));  // Magenta
```

---

## Notes

1. **Tracker required** - Crossline detection needs tracked objects (attach after tracker)
2. **Coordinate system** - Line coordinates are in frame pixels
3. **Thread-safe** - All runtime APIs are thread-safe with mutex
4. **Direction detection** - IN/OUT determined by which side of line target was before/after crossing
