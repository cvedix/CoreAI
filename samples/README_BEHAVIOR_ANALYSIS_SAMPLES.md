# Behavior Analysis Samples

## Tổng quan

Các samples này demo phân tích hành vi phương tiện: crossline detection, stop detection, và traffic jam analysis.

---

## Sample 1: Crossline Detection

### File
`ba_crossline_sample.cpp`

### Mô tả
Đếm số phương tiện vượt qua một đường line dựa trên tracking.

### Pipeline
```
Video → Detector → Tracker → Crossline BA → OSD → [Screen, RTMP]
```

### Features
- Vehicle detection
- Multi-vehicle tracking
- Line crossing detection
- Counting và statistics

### Usage
```bash
./build/bin/ba_crossline_sample
```

### Configuration

#### Define crossing line
```cpp
// Define line coordinates (must be within frame size)
cvedix_objects::cvedix_point start(0, 250);   // Start point
cvedix_objects::cvedix_point end(700, 220);    // End point
cvedix_objects::cvedix_line line(start, end);

// Map line to channel
std::map<int, cvedix_objects::cvedix_line> lines = {
    {0, line}  // Channel 0 -> line
};

auto ba_crossline = std::make_shared<cvedix_ba_crossline_node>(
    "ba_crossline",
    lines
);
```

### Output
- Count of vehicles crossing the line
- Direction detection (left-to-right or right-to-left)
- Real-time statistics

### Customization

#### Multiple lines
```cpp
std::map<int, cvedix_objects::cvedix_line> lines = {
    {0, line1},  // Channel 0 -> line 1
    {1, line2}   // Channel 1 -> line 2
};
```

#### Direction filtering
```cpp
// Only count vehicles crossing in specific direction
// (Implementation depends on node API)
```

---

## Sample 2: Stop Detection

### File
`ba_stop_sample.cpp`

### Mô tả
Phát hiện phương tiện dừng lại trong một khu vực.

### Pipeline
```
Video → Detector → Tracker → Stop BA → OSD → Screen
```

### Features
- Vehicle detection và tracking
- Stop zone definition
- Stop duration calculation
- Alert generation

### Usage
```bash
./build/bin/ba_stop_sample
```

### Configuration

#### Define stop zone
```cpp
// Define stop zone (rectangle or polygon)
cvedix_objects::cvedix_rect stop_zone(x, y, width, height);

auto ba_stop = std::make_shared<cvedix_ba_stop_node>(
    "ba_stop",
    stop_zone,
    min_stop_duration  // Minimum seconds to consider as "stopped"
);
```

### Output
- Vehicles detected in stop zone
- Stop duration for each vehicle
- Alerts for long stops

---

## Sample 3: Traffic Jam Detection

### File
`ba_jam_sample.cpp`

### Mô tả
Phát hiện tắc đường dựa trên mật độ phương tiện và tốc độ.

### Pipeline
```
Video → Detector → Tracker → Jam BA → OSD → Screen
```

### Features
- Vehicle density calculation
- Speed analysis
- Jam zone detection
- Severity level (light/moderate/severe)

### Usage
```bash
./build/bin/ba_jam_sample
```

### Configuration

#### Define analysis zone
```cpp
cvedix_objects::cvedix_rect analysis_zone(x, y, width, height);

auto ba_jam = std::make_shared<cvedix_ba_jam_node>(
    "ba_jam",
    analysis_zone,
    density_threshold,    // Vehicles per area
    speed_threshold,      // Average speed threshold
    min_duration          // Minimum duration for jam
);
```

### Output
- Jam status (normal/jam)
- Vehicle density
- Average speed
- Severity level

---

## Sample 4: RTSP Crossline

### File
`rtsp_ba_crossline_sample.cpp`

### Mô tả
Crossline detection với RTSP stream input.

### Pipeline
```
RTSP Stream → Detector → Tracker → Crossline BA → OSD → Screen
```

### Features
- Real-time RTSP input
- Crossline detection
- Live counting

### Usage
```bash
./build/bin/rtsp_ba_crossline_sample
```

### Configuration
```cpp
auto rtsp_src = std::make_shared<cvedix_rtsp_src_node>(
    "rtsp_src",
    0,
    "rtsp://camera_ip:554/stream"
);
```

---

## So sánh các BA Samples

| Sample | Chức năng | Input | Output |
|--------|-----------|-------|--------|
| **ba_crossline** | Line crossing count | Video file | Count + direction |
| **rtsp_ba_crossline** | Line crossing (RTSP) | RTSP stream | Live count |
| **ba_stop** | Stop detection | Video file | Stop events |
| **ba_jam** | Traffic jam | Video file | Jam status |

---

## Build

```bash
cd build
cmake ..
make ba_crossline_sample
make ba_stop_sample
make ba_jam_sample
make rtsp_ba_crossline_sample
```

---

## Configuration Examples

### Example 1: Single Line
```cpp
cvedix_objects::cvedix_point start(100, 200);
cvedix_objects::cvedix_point end(800, 200);
cvedix_objects::cvedix_line line(start, end);
std::map<int, cvedix_objects::cvedix_line> lines = {{0, line}};
```

### Example 2: Multiple Lines
```cpp
// Line 1: Horizontal
cvedix_objects::cvedix_line line1({0, 300}, {1280, 300});

// Line 2: Diagonal
cvedix_objects::cvedix_line line2({0, 0}, {1280, 720});

std::map<int, cvedix_objects::cvedix_line> lines = {
    {0, line1},
    {1, line2}
};
```

### Example 3: Stop Zone
```cpp
// Define rectangular stop zone
cvedix_objects::cvedix_rect zone(200, 300, 400, 200);

auto ba_stop = std::make_shared<cvedix_ba_stop_node>(
    "ba_stop",
    zone,
    5.0f  // 5 seconds minimum
);
```

---

## Use Cases

### 1. Traffic Counting
```bash
# Count vehicles crossing a line
./build/bin/ba_crossline_sample
```

### 2. Illegal Parking Detection
```bash
# Detect vehicles stopped in no-parking zone
./build/bin/ba_stop_sample
```

### 3. Traffic Management
```bash
# Monitor traffic jam in real-time
./build/bin/ba_jam_sample
```

### 4. Live Monitoring
```bash
# Real-time counting from camera
./build/bin/rtsp_ba_crossline_sample
```

---

## Output Format

### Crossline Output
```json
{
  "channel": 0,
  "line_id": 0,
  "count_left_to_right": 15,
  "count_right_to_left": 8,
  "total_count": 23
}
```

### Stop Output
```json
{
  "channel": 0,
  "vehicle_id": 5,
  "stop_zone": "zone_1",
  "stop_duration": 12.5,
  "alert": true
}
```

### Jam Output
```json
{
  "channel": 0,
  "zone": "zone_1",
  "status": "jam",
  "density": 0.85,
  "average_speed": 5.2,
  "severity": "moderate"
}
```

---

## Performance Tips

1. **Optimize detection** - Use appropriate model size
2. **Adjust tracking** - Balance accuracy vs performance
3. **Zone size** - Smaller zones = faster processing
4. **Frame rate** - Lower FPS for less critical applications

---

## Troubleshooting

### Issue: Incorrect counting
- Verify line coordinates
- Check detection quality
- Adjust tracking parameters

### Issue: False stop detection
- Increase min_stop_duration
- Improve tracking stability
- Filter small movements

### Issue: Jam false positives
- Adjust density threshold
- Tune speed threshold
- Increase min_duration

---

## Related Documentation

- [Vehicle Samples](README_VEHICLE_SAMPLES.md) - Vehicle detection
- [Main README](README.md) - Tổng quan samples




