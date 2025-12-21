/**
 * @file cvedix_stream_status_hookable.h
 * @brief Interface for stream status callbacks from destination nodes
 * 
 * Provides a hookable interface for applications to receive real-time
 * status updates from destination nodes (FPS, latency, resolution).
 * 
 * @section status_usage Usage Example
 * @code
 * auto des = std::make_shared<cvedix_screen_des_node>("des", 0);
 * 
 * // Set callback to receive stream status
 * des->set_stream_status_hooker([](std::string node_name, cvedix_stream_status status) {
 *     std::cout << "Output " << node_name << ": "
 *               << status.fps << " FPS, latency: " << status.latency << "ms"
 *               << std::endl;
 * });
 * @endcode
 * 
 * @see cvedix_des_node Destination nodes inherit this interface
 * @see cvedix_stream_info_hookable For source node info
 */

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <memory>

namespace cvedix_nodes {

    /**
     * @brief Stream status structure from destination nodes
     * 
     * Contains real-time statistics about the output stream.
     * Values may differ from source due to processing/resizing.
     */
    struct cvedix_stream_status {
        int channel_index = -1;  ///< Channel index this stream belongs to
        int frame_index = -1;    ///< Latest processed frame index

        int latency = 0;         ///< Processing latency in milliseconds (from src to des)

        float fps = 0;           ///< Output FPS (may differ from source)
        int width = 0;           ///< Output width (may differ from source due to resize)
        int height = 0;          ///< Output height (may differ from source due to resize)
        std::string direction;   ///< Output destination description (screen, file, rtmp, etc.)
    };

    /**
     * @brief Callback type for stream status notifications
     * 
     * @param node_name Name of the destination node
     * @param stream_status Status information structure
     * 
     * @warning Callback must not block - execute quickly or dispatch to another thread
     */
    typedef std::function<void(std::string, cvedix_stream_status)> cvedix_stream_status_hooker;

    /**
     * @brief Mixin class providing stream status hook functionality
     * 
     * Inherited by cvedix_des_node to allow external code to receive
     * real-time status updates about stream output.
     * 
     * @note Thread-safe - hooker invocation is protected by mutex
     * 
     * @see cvedix_des_node Primary user of this interface
     */
    class cvedix_stream_status_hookable
    {
    private:
        /* data */

    protected:
        /// @brief Mutex protecting the hooker callback
        std::mutex stream_status_hooker_lock;
        /// @brief Registered callback for stream status events
        cvedix_stream_status_hooker stream_status_hooker;

    public:
        /// @brief Default constructor
        cvedix_stream_status_hookable(/* args */) {}
        /// @brief Destructor
        ~cvedix_stream_status_hookable() {}
        
        /**
         * @brief Register a callback for stream status events
         * 
         * @param stream_status_hooker Callback function to invoke on status updates
         */
        void set_stream_status_hooker(cvedix_stream_status_hooker stream_status_hooker) {
            std::lock_guard<std::mutex> guard(stream_status_hooker_lock);
            this->stream_status_hooker = stream_status_hooker;
        }

        /**
         * @brief Invoke the registered stream status callback
         * 
         * Called internally by destination nodes when outputting frames.
         * 
         * @param node_name Name of the destination node
         * @param stream_status Status information to report
         */
        void invoke_stream_status_hooker(std::string node_name, cvedix_stream_status stream_status) {
            std::lock_guard<std::mutex> guard(stream_status_hooker_lock);
            if (this->stream_status_hooker) {
                this->stream_status_hooker(node_name, stream_status);
            }
        }
    };
}