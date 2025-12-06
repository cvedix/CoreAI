# Destination Nodes (des/)

## Overview

Destination nodes are the exit points of a pipeline. They receive frame metadata from upstream nodes and write/output the data to various destinations (files, network streams, displays, applications). All destination nodes inherit from `cvedix_des_node` (defined in `../common/`).

**Important**: Destination nodes **do NOT support multi-channel** by default. You **MUST** specify a `channel_index` when creating instances, with each instance handling one channel.

## Node List

- **cvedix_file_des_node**: Saves video streams to local files
  - Automatic file naming with timestamps
  - Configurable max duration per file
  - Supports hardware encoding via GStreamer
  - Optional OSD (On-Screen Display) rendering

- **cvedix_image_des_node**: Saves frames as image files
  - Can save to directories or network sockets
  - Supports JPEG, PNG formats

- **cvedix_screen_des_node**: Displays video on local screen/window
  - Uses GStreamer for rendering
  - Automatically selects sink based on environment:
    - `CVEDIX_SCREEN_SINK`: Custom sink override
    - `DISPLAY`: Uses `fpsdisplaysink video-sink=ximagesink`
    - `WAYLAND_DISPLAY`: Uses `waylandsink`
    - Fallback: `kmssink` (for embedded systems)

- **cvedix_rtmp_des_node**: Pushes video streams to RTMP servers
  - Live streaming support
  - Configurable bitrate and encoding

- **cvedix_rtsp_des_node**: Acts as RTSP server (requires `gstreamer-rtsp-server`)
  - Built-in RTSP server, no external server needed
  - Clients can connect via RTSP URL
  - Requires `libgstrtspserver-1.0-dev` and `gstreamer1.0-rtsp` packages

- **cvedix_app_des_node**: Sends frame data to external applications
  - Allows integration with custom applications
  - Uses shared memory or inter-process communication

- **cvedix_fake_des_node**: Virtual destination node (does nothing)
  - Useful for testing or dropping frames silently

## Common Features

All destination nodes share these characteristics:

1. **Channel Index**: Required at construction, one channel per instance
2. **OSD Support**: Can render overlay graphics on frames before output
3. **Resolution Control**: Can resize frames before writing/outputting
4. **Frame Preparation**: Uses `utils::prepare_output_frame()` from `../common/frame_utils.h` for consistent frame handling

## Frame Preparation Utility

The `utils::prepare_output_frame()` helper function (in `../common/frame_utils.h`) handles:
- Selecting between OSD frame and original frame based on `osd` parameter
- Resizing frames to target resolution if needed
- This is used consistently across all destination nodes

## Creating a New Destination Node

To create a custom destination node:

1. Inherit from `cvedix_des_node`:
   ```cpp
   #include "../common/cvedix_des_node.h"
   #include "../common/frame_utils.h"
   
   class my_custom_des_node : public cvedix_des_node {
       // ...
   };
   ```

2. Call the base constructor with `node_name` and `channel_index`:
   ```cpp
   my_custom_des_node::my_custom_des_node(
       std::string node_name, int channel_index, ...)
       : cvedix_des_node(node_name, channel_index) {
       // initialization
   }
   ```

3. Implement `handle_frame_meta()` to process frames:
   ```cpp
   std::shared_ptr<cvedix_objects::cvedix_meta>
   my_custom_des_node::handle_frame_meta(
       std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
       
       // Prepare frame (handles OSD and resizing)
       cv::Mat output_frame = utils::prepare_output_frame(
           meta, use_osd, target_resolution);
       
       // Write/output the frame
       // ... your output logic ...
       
       return nullptr; // Des nodes typically don't push further
   }
   ```

4. Optionally implement `handle_control_meta()` for control messages

## Related Files

- Base class: `../common/cvedix_des_node.h`
- Node base: `../common/cvedix_node.h`
- Frame utilities: `../common/frame_utils.h`

