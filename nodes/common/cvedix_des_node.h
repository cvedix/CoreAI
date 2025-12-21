/**
 * @file cvedix_des_node.h
 * @brief Base class for all destination nodes in Core AI Runtime
 * 
 * This file defines the cvedix_des_node base class which serves as the end point
 * for video analytics pipelines. Destination nodes consume processed frame_meta
 * and output results to various targets (screen, file, network, etc.).
 * 
 * @section des_implementations Available Implementations
 * - **cvedix_screen_des_node**: Display on screen/window
 * - **cvedix_rtmp_des_node**: Push to RTMP server
 * - **cvedix_file_des_node**: Save to video file
 * - **cvedix_null_des_node**: Discard output (for benchmarking)
 * 
 * @section des_status Status Reporting
 * Destination nodes can report real-time status (FPS, latency, resolution)
 * via the cvedix_stream_status_hookable interface.
 * 
 * @section des_usage Usage Example
 * @code
 * // Create a screen destination on channel 0
 * auto des = std::make_shared<cvedix_screen_des_node>("des", 0);
 * 
 * // Optionally monitor output status
 * des->set_stream_status_hooker([](std::string name, cvedix_stream_status status) {
 *     std::cout << "FPS: " << status.fps << ", latency: " << status.latency << "ms" << std::endl;
 * });
 * 
 * // Build pipeline
 * des->attach_to({osd_node});
 * @endcode
 * 
 * @see cvedix_node Base class
 * @see cvedix_src_node Source node counterpart
 * @see cvedix_stream_status_hookable For status callbacks
 */

#pragma once

#include <string>

#include "cvedix_node.h"
#include "cvedix_stream_status_hookable.h"

namespace cvedix_nodes {

    /**
     * @brief Base class for all destination nodes
     * 
     * Destination nodes are the endpoints of video analytics pipelines.
     * They consume processed frame_meta and output to external targets.
     * 
     * @section des_features Key Features
     * - **Single Channel**: Each destination operates on one channel
     * - **Status Reporting**: Real-time FPS and latency monitoring
     * - **No Output**: Does not push meta to next nodes (terminal)
     * 
     * @note This is an abstract base class. Use concrete implementations
     *       like cvedix_screen_des_node or cvedix_rtmp_des_node.
     * 
     * @see cvedix_node Base class
     * @see cvedix_stream_status_hookable For status callbacks
     */
    class cvedix_des_node: public cvedix_node, public cvedix_stream_status_hookable {
    private:
        /// @brief Cached stream status for this destination
        cvedix_stream_status stream_status;

        /// @brief FPS calculation window in milliseconds (default: 500)
        int fps_epoch = 500;
        /// @brief Frame counter for FPS calculation
        int fps_counter = 0;
        /// @brief Timestamp for FPS calculation
        std::chrono::system_clock::time_point fps_last_time;

    protected:
        /**
         * @brief Disabled in destination nodes
         * 
         * Destination nodes don't dispatch to next nodes.
         * Marked final to prevent override.
         */
        virtual void dispatch_run() override final;

        /**
         * @brief Process incoming frame meta
         * 
         * Base implementation returns nullptr (no further dispatch).
         * Override in derived classes to implement output logic.
         * 
         * @param meta Frame meta to output
         * @return nullptr (destination nodes don't forward)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 

        /**
         * @brief Process incoming control meta
         * 
         * Base implementation returns nullptr.
         * 
         * @param meta Control meta to process
         * @return nullptr (destination nodes don't forward)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

        /**
         * @brief Protected constructor
         * 
         * @param node_name Unique node identifier
         * @param channel_index Video channel this destination handles
         */
        cvedix_des_node(std::string node_name, int channel_index);

    public:
        /// @brief Destructor
        ~cvedix_des_node();

        /**
         * @brief Returns DES node type
         * @return cvedix_node_type::DES
         */
        virtual cvedix_node_type node_type() override;

        /// @brief Channel index this destination is bound to
        int channel_index;
    };
}