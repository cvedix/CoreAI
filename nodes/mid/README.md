# Middle Nodes (mid/)

## Overview

Middle nodes are processing nodes that sit between source and destination nodes in a pipeline. They receive metadata from upstream nodes, process it, and forward the results to downstream nodes. Middle nodes inherit from `cvedix_node` (defined in `../common/`).

**Important**: Most middle nodes **support multi-channel** by default, meaning they can handle frames from multiple channels in a single instance. However, check individual node documentation for specific behavior.

## Node List

- **cvedix_split_node**: Splits pipeline into multiple branches
  - **split_with_channel_index** (default: false): If true, routes meta to next nodes based on matching channel index
  - **split_with_deep_copy** (default: false): If true, creates deep copies of metadata for thread-safety
  - More flexible than default node splitting behavior
  - Note: Deep copy mode impacts performance

- **cvedix_sync_node**: Synchronizes pipeline branches
  - Ensures frames from different branches are processed together
  - Useful when merging parallel processing branches

- **cvedix_message_broker_node**: Base class for message broker nodes
  - Handles forwarding metadata to multiple subscribers
  - See `../broker/` directory for concrete implementations

- **cvedix_placeholder_node**: Virtual middle node (does nothing, just forwards)
  - Useful for pipeline structure or future expansion
  - Passes metadata through unchanged

- **cvedix_skip_node**: Conditionally skips frames based on logic
  - Can be used for frame rate reduction or conditional processing

## Default Node Splitting Behavior

All non-destination nodes can split pipelines by default:
- By default, they **copy pointer** of metadata and push to all connected next nodes
- Each next node handles the **same metadata** (not thread-safe if modified)
- Each next node receives **equal number** of metadata items

`cvedix_split_node` provides additional control with its configuration options.

## Multi-Channel Support

Most middle nodes support multi-channel processing:
- They can handle frames from different channels in a single instance
- Some nodes use `std::map<int, ...>` internally to maintain separate state per channel
- For better performance, consider using separate instances per channel for parallel processing

## Creating a New Middle Node

To create a custom middle node:

1. Inherit from `cvedix_node`:
   ```cpp
   #include "../common/cvedix_node.h"
   
   class my_custom_mid_node : public cvedix_node {
       // ...
   };
   ```

2. Call the base constructor:
   ```cpp
   my_custom_mid_node::my_custom_mid_node(std::string node_name)
       : cvedix_node(node_name) {
       // initialization
   }
   ```

3. Implement `handle_frame_meta()` to process frames:
   ```cpp
   std::shared_ptr<cvedix_objects::cvedix_meta>
   my_custom_mid_node::handle_frame_meta(
       std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override {
       
       // Process the frame/metadata
       // ... your processing logic ...
       
       // Optionally modify metadata
       // ... modifications ...
       
       // Return processed metadata (will be pushed to next nodes)
       return meta;
   }
   ```

4. If you need custom splitting behavior, override `push_meta()`:
   ```cpp
   void my_custom_mid_node::push_meta(
       std::shared_ptr<cvedix_objects::cvedix_meta> meta) override {
       // Custom routing logic
       // Call base class or implement custom forwarding
   }
   ```

5. For batch processing, set `frame_meta_handle_batch > 1` and implement `handle_frame_meta_by_batch()`

## Related Files

- Node base: `../common/cvedix_node.h`
- Meta publisher/subscriber: `../common/cvedix_meta_publisher.h`, `../common/cvedix_meta_subscriber.h`

