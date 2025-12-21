/**
 * @file cvedix_node.h
 * @brief Base class definition for all pipeline nodes in Core AI Runtime
 * 
 * This file defines the fundamental cvedix_node class which serves as the base
 * for all processing nodes in the video analytics pipeline. All nodes (source,
 * destination, and middle nodes) inherit from this class.
 * 
 * @section node_types Node Types
 * - **SRC**: Source nodes that generate data (no input branches)
 * - **DES**: Destination nodes that consume data (no output branches)  
 * - **MID**: Middle nodes that process and forward data
 * 
 * @section threading Threading Model
 * Each node runs two internal threads:
 * - **handle_thread**: Processes incoming meta from in_queue
 * - **dispatch_thread**: Dispatches processed meta to next nodes
 * 
 * @section usage Usage Example
 * @code
 * // Create nodes
 * auto src = std::make_shared<cvedix_file_src_node>("src", 0, "video.mp4");
 * auto detector = std::make_shared<cvedix_yolo_detector_node>("detector", ...);
 * auto des = std::make_shared<cvedix_screen_des_node>("des", 0);
 * 
 * // Build pipeline using attach_to
 * detector->attach_to({src});
 * des->attach_to({detector});
 * @endcode
 * 
 * @see cvedix_src_node Base class for source nodes
 * @see cvedix_des_node Base class for destination nodes
 * @see cvedix_infer_node Base class for inference nodes
 */

#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <memory>
#include <chrono>

#include "cvedix/utils/cvedix_semaphore.h"
#include "cvedix/utils/cvedix_utils.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix_meta_publisher.h"
#include "cvedix_meta_hookable.h"
#include "cvedix/objects/cvedix_control_meta.h"
#include "cvedix/objects/cvedix_frame_meta.h"
#include "cvedix/excepts/cvedix_invalid_calling_error.h"

namespace cvedix_nodes {

    /**
     * @brief Enumeration of node types in the pipeline
     * 
     * Defines the three fundamental types of nodes based on their
     * position and role in the processing pipeline.
     */
    enum cvedix_node_type {
        SRC,  ///< Source node - generates data, must not have input branches
        DES,  ///< Destination node - consumes data, must not have output branches
        MID   ///< Middle node - processes data, can have both input and output branches
    };

    /**
     * @brief Base class for all pipeline nodes
     * 
     * cvedix_node is the fundamental building block of the video analytics pipeline.
     * It provides the core infrastructure for:
     * - Receiving and dispatching metadata between nodes
     * - Thread management for asynchronous processing
     * - Queue-based buffering for flow control
     * - Node attachment/detachment for dynamic pipeline construction
     * 
     * @note This class cannot be instantiated directly. Use derived classes
     *       such as cvedix_src_node, cvedix_des_node, or cvedix_infer_node.
     * 
     * @note Most nodes support multi-channel operation. However, source and
     *       destination nodes require channel index specification at construction.
     * 
     * @see cvedix_meta_publisher For meta publishing interface
     * @see cvedix_meta_subscriber For meta subscription interface
     * @see cvedix_meta_hookable For meta hooking interface
     */
    class cvedix_node: public cvedix_meta_publisher, 
                    public cvedix_meta_subscriber, 
                    public cvedix_meta_hookable, 
                    public std::enable_shared_from_this<cvedix_node> {
    private:
        /// @brief Vector of previous nodes in the pipeline
        std::vector<std::shared_ptr<cvedix_node>> pre_nodes;

        /// @brief Thread for handling incoming meta
        std::thread handle_thread;
        /// @brief Thread for dispatching meta to next nodes
        std::thread dispatch_thread;

    protected:
        /// @brief Flag indicating if node is alive and processing
        bool alive = true;

        /// @brief Maximum size for input queue (default: 50)
        int max_in_queue_size = 50;

        /**
         * @brief Batch size for frame meta handling
         * 
         * By default (=1), frame meta is handled one by one.
         * Setting > 1 enables batch mode, calling handle_frame_meta(vector<>) instead.
         * 
         * @note Control meta is always handled one by one regardless of this setting.
         */
        int frame_meta_handle_batch = 1;

        /// @brief Queue for incoming meta from previous nodes
        std::queue<std::shared_ptr<cvedix_objects::cvedix_meta>> in_queue;
        /// @brief Mutex for thread-safe access to in_queue
        std::mutex in_queue_lock;
        /// @brief Queue for outgoing meta to next nodes
        std::queue<std::shared_ptr<cvedix_objects::cvedix_meta>> out_queue;

        /// @brief Semaphore for synchronizing in_queue access
        cvedix_utils::cvedix_semaphore in_queue_semaphore;
        /// @brief Semaphore for synchronizing out_queue access
        cvedix_utils::cvedix_semaphore out_queue_semaphore;

        /**
         * @brief Main processing loop for handling incoming meta
         * 
         * Gets meta from in_queue, processes it, and puts results into out_queue.
         * Source nodes should override this to generate meta instead of receiving.
         */
        virtual void handle_run();

        /**
         * @brief Main dispatch loop for sending meta to next nodes
         * 
         * Gets meta from out_queue and pushes to subscribed next nodes.
         * Destination nodes should override this to do nothing (no next nodes).
         */
        virtual void dispatch_run();

        /**
         * @brief Handle a single frame meta object
         * 
         * Override this method to define custom frame processing logic.
         * This is called when frame_meta_handle_batch == 1 (default).
         * 
         * @param meta The frame meta to process
         * @return Processed meta to pass to next nodes, or nullptr to stop propagation
         * 
         * @note Destination nodes typically return nullptr
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);

        /**
         * @brief Handle a single control meta object
         * 
         * Override this method to define custom control message handling.
         * 
         * @param meta The control meta to process
         * @return Processed meta to pass to next nodes, or nullptr to stop propagation
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta);

        /**
         * @brief Handle frame meta objects in batch mode
         * 
         * Override this method when frame_meta_handle_batch > 1.
         * Useful for inference nodes that benefit from batch processing.
         * 
         * @param meta_with_batch Vector of frame metas to process as a batch
         */
        virtual void handle_frame_meta(const std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_meta>>& meta_with_batch);

        /**
         * @brief Signal that node initialization is complete
         * 
         * Must be called by derived classes after all resources are initialized
         * (typically at the end of the constructor chain). Starts internal threads.
         */
        void initialized();

        /**
         * @brief Signal that node is being destroyed
         * 
         * Called by derived classes before resources are destroyed
         * (typically at the start of the destructor chain). Stops internal threads.
         */
        virtual void deinitialized();

        /**
         * @brief Queue meta for dispatch to next nodes
         * 
         * Adds meta to out_queue for orderly dispatch by dispatch_run().
         * Different from push_meta() which dispatches directly.
         * 
         * @param meta The meta object to queue for dispatch
         * 
         * @warning Must only be called from handle_run() thread context
         */
        void pendding_meta(std::shared_ptr<cvedix_objects::cvedix_meta> meta);

        /**
         * @brief Protected constructor
         * 
         * @param node_name Unique identifier for this node (e.g., "detector_0")
         * 
         * @note Protected to prevent direct instantiation - use derived classes
         */
        cvedix_node(std::string node_name);

    public:
        /// @brief Virtual destructor
        virtual ~cvedix_node();

        /**
         * @brief Unique identifier for this node
         * 
         * Format: "<type>_<channel>", e.g., "file_src_0", "detector_1"
         */
        std::string node_name;

        /**
         * @brief Receive and process meta from previous nodes
         * 
         * Entry point for incoming meta. Can be overridden in derived classes
         * to hook/modify meta before standard processing.
         * 
         * @param meta The incoming meta object
         * 
         * @note When overriding, always call base cvedix_node::meta_flow() after custom logic
         */
        virtual void meta_flow(std::shared_ptr<cvedix_objects::cvedix_meta> meta) override;

        /**
         * @brief Get the type of this node
         * 
         * @return cvedix_node_type SRC, DES, or MID
         */
        virtual cvedix_node_type node_type();
        
        /**
         * @brief Detach this node from all previous nodes
         * 
         * Removes this node from the subscriber list of all previous nodes.
         * After calling, this node will no longer receive meta from any source.
         */
        void detach();

        /**
         * @brief Detach this node from specific previous nodes
         * 
         * @param pre_node_names Names of previous nodes to detach from
         */
        void detach_from(std::vector<std::string> pre_node_names);

        /**
         * @brief Recursively detach this node and all downstream nodes
         * 
         * Detaches this node from previous nodes, then does the same for all
         * next nodes recursively. Useful for clean pipeline teardown.
         */
        void detach_recursively();

        /**
         * @brief Attach this node to previous nodes in the pipeline
         * 
         * Subscribes this node to receive meta from the specified previous nodes.
         * This is the primary method for building pipelines.
         * 
         * @param pre_nodes Vector of nodes to attach to (receive meta from)
         * 
         * @code
         * // Build a simple pipeline
         * detector->attach_to({source});
         * osd->attach_to({detector});
         * screen->attach_to({osd});
         * @endcode
         */
        void attach_to(std::vector<std::shared_ptr<cvedix_node>> pre_nodes);

        /**
         * @brief Get all nodes that this node dispatches to
         * 
         * @return Vector of next nodes in the pipeline
         */
        std::vector<std::shared_ptr<cvedix_node>> next_nodes();

        /**
         * @brief Get a human-readable description of this node
         * 
         * @return String representation including node name and configuration
         */
        virtual std::string to_string();
    };

}