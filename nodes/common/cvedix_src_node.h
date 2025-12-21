/**
 * @file cvedix_src_node.h
 * @brief Base class for all source nodes in Core AI Runtime
 * 
 * This file defines the cvedix_src_node base class which serves as the entry point
 * for all video analytics pipelines. Source nodes are responsible for:
 * - Reading video/image data from various sources (files, cameras, network streams)
 * - Creating cvedix_frame_meta objects containing frame data
 * - Controlling pipeline lifecycle (start/stop)
 * 
 * @section src_implementations Available Implementations
 * - **cvedix_file_src_node**: Read from video files
 * - **cvedix_rtsp_src_node**: Read from RTSP network streams
 * - **cvedix_udp_src_node**: Read from UDP streams
 * - **cvedix_app_src_node**: Receive frames from application code
 * - **cvedix_image_src_node**: Read from image files
 * 
 * @section src_channel Channel Assignment
 * Each source node is bound to a specific channel index at construction.
 * Multiple source nodes can feed into the same pipeline using different channels.
 * 
 * @section src_usage Usage Example
 * @code
 * // Create a file source on channel 0
 * auto src = std::make_shared<cvedix_file_src_node>("src", 0, "video.mp4");
 * 
 * // Build pipeline
 * detector->attach_to({src});
 * 
 * // Start the pipeline
 * src->start();
 * 
 * // ... processing ...
 * 
 * // Stop the pipeline
 * src->stop();
 * @endcode
 * 
 * @see cvedix_node Base class
 * @see cvedix_des_node Destination node counterpart
 */

#pragma once

#include "cvedix_node.h"
#include "cvedix_stream_info_hookable.h"
#include "cvedix/excepts/cvedix_not_implemented_error.h"
#include "cvedix/excepts/cvedix_invalid_calling_error.h"
#include "cvedix/utils/cvedix_gate.h"

namespace cvedix_nodes {

    /**
     * @brief Base class for all source nodes
     * 
     * Source nodes are the starting point of video analytics pipelines.
     * They generate cvedix_frame_meta objects containing video frames
     * and push them to downstream processing nodes.
     * 
     * @section src_features Key Features
     * - **Single Channel**: Each source node operates on one channel
     * - **Controllable**: start()/stop() methods control data generation
     * - **Stream Info**: Provides original video dimensions and FPS
     * - **Resize Support**: Optional frame resizing for performance
     * 
     * @note This is an abstract base class. Use concrete implementations
     *       like cvedix_file_src_node or cvedix_rtsp_src_node.
     * 
     * @see cvedix_node Base class
     * @see cvedix_stream_info_hookable For stream info callbacks
     */
    class cvedix_src_node: public cvedix_node, public cvedix_stream_info_hookable {
    private:
        /* data */
    
    protected:
        /**
         * @brief Main data generation loop
         * 
         * Must be overridden by derived classes to implement data reading.
         * Should generate frame_meta objects and dispatch them.
         */
        virtual void handle_run() override;

        /**
         * @brief Not used in source nodes
         * @note Source nodes generate frames, not consume them
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 

        /**
         * @brief Not used in source nodes
         * @note Source nodes generate control meta, not consume them
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

        /**
         * @brief Protected constructor
         * 
         * @param node_name Unique node identifier
         * @param channel_index Video channel index this source operates on
         * @param resize_ratio Resize factor for output frames (default: 1.0 = no resize)
         */
        cvedix_src_node(std::string node_name, 
                    int channel_index, 
                    float resize_ratio = 1.0);

        /// @brief Original video FPS (-1 if unknown)
        int original_fps = -1;
        /// @brief Original video width in pixels
        int original_width = 0;
        /// @brief Original video height in pixels
        int original_height = 0;

        /// @brief Current frame index (starts at 0)
        int frame_index;
        /// @brief Channel index this source is bound to
        int channel_index;
        /// @brief Resize ratio for output frames
        float resize_ratio;

        /**
         * @brief Gate for start/stop control
         * 
         * Derived classes should check this gate in handle_run() to
         * properly respond to start()/stop() commands.
         */
        cvedix_utils::cvedix_gate gate;

        /**
         * @brief Cleanup on destruction
         * 
         * Sends dead signal to notify downstream nodes.
         */
        virtual void deinitialized() override;

    public:
        /// @brief Destructor
        ~cvedix_src_node();

        /**
         * @brief Returns SRC node type
         * @return cvedix_node_type::SRC
         */
        virtual cvedix_node_type node_type() override;

        /**
         * @brief Start the pipeline
         * 
         * Opens the gate and begins frame generation.
         * Downstream nodes will start receiving frame_meta.
         */
        void start();

        /**
         * @brief Stop the pipeline
         * 
         * Closes the gate and stops frame generation.
         * Sends stop signal to downstream nodes.
         */
        void stop();

        /**
         * @brief Debug: Print status of all pipeline nodes
         * 
         * Sends a speak signal through the pipeline.
         * Each node prints its current status.
         */
        void speak();

        /**
         * @brief Debug: Manually trigger video recording
         * 
         * @param osd Include OSD overlay in recording (default: false)
         * @param video_duration Recording duration in seconds (default: 10)
         * 
         * @note This is a debug API. Normally recording is triggered
         *       automatically by behavior analysis nodes.
         */
        void record_video_manually(bool osd = false, int video_duration = 10);

        /**
         * @brief Debug: Manually trigger image capture
         * 
         * @param osd Include OSD overlay in capture (default: false)
         * 
         * @note This is a debug API. Normally capture is triggered
         *       automatically by behavior analysis nodes.
         */
        void record_image_manually(bool osd = false);

        /**
         * @brief Get original video FPS
         * @return FPS value, or -1 if unknown
         */
        int get_original_fps() const;

        /**
         * @brief Get original video width
         * @return Width in pixels
         */
        int get_original_width() const;

        /**
         * @brief Get original video height
         * @return Height in pixels
         */
        int get_original_height() const;
    };
    
}