# Common Base Classes and Utilities (common/)

## Overview

This directory contains the foundational base classes, interfaces, and utilities used across all node types in the pipeline system. These provide the core infrastructure for node communication, metadata handling, and common operations.

## Base Classes

### cvedix_node

The root base class for all nodes in the pipeline system.

**Key Features**:
- Manages connections to previous and next nodes
- Handles thread-based processing (`handle_thread` and `dispatch_thread`)
- Provides input/output queues with thread synchronization
- Supports batch processing via `frame_meta_handle_batch`
- Implements publisher/subscriber pattern for metadata

**Key Methods**:
- `connect()`: Connect to next node
- `handle_frame_meta()`: Override to process frame metadata (virtual)
- `handle_control_meta()`: Override to process control metadata (virtual)
- `push_meta()`: Push metadata to connected next nodes

**Node Types**:
- `SRC`: Source nodes (no input branches)
- `DES`: Destination nodes (no output branches)
- `MID`: Middle nodes (can have both input and output branches)

### cvedix_src_node

Base class for all source nodes. Inherits from `cvedix_node` and `cvedix_stream_info_hookable`.

**Key Features**:
- Requires `channel_index` at construction
- Implements `handle_run()` for continuous frame reading
- Manages stream information (fps, resolution)
- Provides gate control (`start()`, `stop()`)

**Protected Members**:
- `original_fps`, `original_width`, `original_height`: Stream information
- `frame_index`, `channel_index`, `resize_ratio`: Channel configuration
- `gate`: Control mechanism for start/stop

### cvedix_des_node

Base class for all destination nodes. Inherits from `cvedix_node`.

**Key Features**:
- Requires `channel_index` at construction
- Typically returns `nullptr` from `handle_frame_meta()` (doesn't forward)
- Processes frames for output/writing

## Meta Communication

### cvedix_meta_publisher

Interface for nodes that publish metadata to subscribers.

### cvedix_meta_subscriber

Interface for nodes that subscribe to metadata from publishers.

### cvedix_meta_hookable

Interface for nodes that can register hooks to receive metadata notifications.

## Hook Interfaces

### cvedix_stream_info_hookable

Interface for receiving stream information callbacks (fps, resolution).
- Used by source nodes to notify about stream characteristics

### cvedix_stream_status_hookable

Interface for receiving stream status callbacks (connected, disconnected, etc.).

## Utilities

### frame_utils.h

Contains utility functions for frame processing:

- **`select_source_frame()`**: Selects between OSD frame and original frame based on flag
- **`prepare_output_frame()`**: Prepares frame for output with OSD and optional resizing
  - Used consistently across all destination nodes
  - Handles selecting source frame (OSD vs original)
  - Resizes to target resolution if specified

**Usage Example**:
```cpp
#include "frame_utils.h"

cv::Mat output = utils::prepare_output_frame(meta, use_osd, target_size);
```

## Creating Custom Nodes

### Source Node
1. Inherit from `cvedix_src_node`
2. Implement `handle_run()` to read and push frames
3. Set `channel_index` in constructor

### Destination Node
1. Inherit from `cvedix_des_node`
2. Implement `handle_frame_meta()` to process and output frames
3. Use `utils::prepare_output_frame()` for consistent frame handling
4. Set `channel_index` in constructor

### Middle Node
1. Inherit from `cvedix_node`
2. Implement `handle_frame_meta()` to process frames
3. Return modified or original metadata to forward to next nodes

## Related Directories

- Source nodes: `../src/`
- Destination nodes: `../des/`
- Middle nodes: `../mid/`
- Inference nodes: `../infers/base/`

