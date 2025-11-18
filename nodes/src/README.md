# Source Nodes (src/)

## Overview

Source nodes are the entry points of a pipeline. They read data from various sources (files, network streams, applications) and push frame metadata to downstream nodes. All source nodes inherit from `cvedix_src_node` (defined in `../common/`).

**Important**: Source nodes **do NOT support multi-channel** by default. You **MUST** specify a `channel_index` when creating instances, with each instance handling one channel.

## Node List

- **cvedix_file_src_node**: Reads video from local files (mp4, mkv, avi, etc.)
  - Supports hardware decoding via GStreamer
  - Can cycle playback when end-of-file is reached
  - Optional frame skipping

- **cvedix_image_src_node**: Reads images from files or network sockets
  - Supports continuous reading from directories
  - Can read from TCP/UDP sockets

- **cvedix_rtsp_src_node**: Reads RTSP network streams
  - Uses GStreamer for demuxing and decoding
  - Handles network reconnection

- **cvedix_rtmp_src_node**: Reads RTMP network streams
  - Similar to RTSP but for RTMP protocol

- **cvedix_udp_src_node**: Reads UDP network streams
  - Low-latency streaming support

- **cvedix_app_src_node**: Receives frame data from external applications
  - Allows integration with custom applications
  - Uses shared memory or inter-process communication

## Common Features

All source nodes share these characteristics:

1. **Channel Index**: Required at construction, one channel per instance
2. **Resize Ratio**: Optional frame resizing before processing
3. **Stream Info Hooks**: Can register hooks to receive stream information (fps, resolution)
4. **Gate Control**: Can be started/stopped via `start()` and `stop()` methods

## Creating a New Source Node

To create a custom source node:

1. Inherit from `cvedix_src_node`:
   ```cpp
   #include "../common/cvedix_src_node.h"
   
   class my_custom_src_node : public cvedix_src_node {
       // ...
   };
   ```

2. Call the base constructor with `node_name` and `channel_index`:
   ```cpp
   my_custom_src_node::my_custom_src_node(std::string node_name, int channel_index)
       : cvedix_src_node(node_name, channel_index, resize_ratio) {
       // initialization
   }
   ```

3. Implement `handle_run()` to read frames and push metadata:
   ```cpp
   void my_custom_src_node::handle_run() {
       while (alive && gate.is_open()) {
           // Read frame from source
           cv::Mat frame;
           // ... read logic ...
           
           // Create and push frame metadata
           auto meta = std::make_shared<cvedix_objects::cvedix_frame_meta>(
               frame_index++, channel_index, frame);
           push_meta(meta);
       }
   }
   ```

4. Optionally implement `to_string()` for logging/debugging

## Related Files

- Base class: `../common/cvedix_src_node.h`
- Node base: `../common/cvedix_node.h`
- Stream info hooks: `../common/cvedix_stream_info_hookable.h`

