/**
 * @file cvedix_stream_info_hookable.h
 * @brief Interface for stream information callbacks from source nodes
 * 
 * Provides a hookable interface for applications to receive notifications
 * when stream information (resolution, FPS, URI) becomes available from source nodes.
 * 
 * @section info_usage Usage Example
 * @code
 * auto src = std::make_shared<cvedix_file_src_node>("src", 0, "video.mp4");
 * 
 * // Set callback to receive stream info
 * src->set_stream_info_hooker([](std::string node_name, cvedix_stream_info info) {
 *     std::cout << "Stream from " << node_name << ": "
 *               << info.original_width << "x" << info.original_height
 *               << " @ " << info.original_fps << " FPS" << std::endl;
 * });
 * @endcode
 * 
 * @see cvedix_src_node Source nodes inherit this interface
 * @see cvedix_stream_status_hookable For destination node status
 */

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <memory>

namespace cvedix_nodes {

    /**
     * @brief Stream information structure from source nodes
     * 
     * Contains metadata about the input video stream.
     */
    struct cvedix_stream_info {
        int channel_index = -1;   ///< Channel index this stream belongs to
        int original_fps = 0;     ///< Original frame rate of the stream
        int original_width = 0;   ///< Original width in pixels
        int original_height = 0;  ///< Original height in pixels
        std::string uri = "";     ///< URI/path of the stream source
    };
    
    /**
     * @brief Callback type for stream info notifications
     * 
     * @param node_name Name of the source node
     * @param stream_info Stream information structure
     * 
     * @warning Callback must not block - execute quickly or dispatch to another thread
     */
    typedef std::function<void(std::string, cvedix_stream_info)> cvedix_stream_info_hooker;

    /**
     * @brief Mixin class providing stream info hook functionality
     * 
     * Inherited by cvedix_src_node to allow external code to receive
     * notifications when stream information becomes available.
     * 
     * @note Thread-safe - hooker invocation is protected by mutex
     * 
     * @see cvedix_src_node Primary user of this interface
     */
    class cvedix_stream_info_hookable
    {
    private:
        /* data */

    protected:
        /// @brief Mutex protecting the hooker callback
        std::mutex stream_info_hooker_lock;
        /// @brief Registered callback for stream info events
        cvedix_stream_info_hooker stream_info_hooker;

    public:
        /// @brief Default constructor
        cvedix_stream_info_hookable(/* args */) {}
        /// @brief Destructor
        ~cvedix_stream_info_hookable() {}

        /**
         * @brief Register a callback for stream info events
         * 
         * @param stream_info_hooker Callback function to invoke when info is available
         */
        void set_stream_info_hooker(cvedix_stream_info_hooker stream_info_hooker) {
            std::lock_guard<std::mutex> guard(stream_info_hooker_lock);
            this->stream_info_hooker = stream_info_hooker;
        }

        /**
         * @brief Invoke the registered stream info callback
         * 
         * Called internally by source nodes when stream info is available.
         * 
         * @param node_name Name of the source node
         * @param stream_info Stream information to report
         */
        void invoke_stream_info_hooker(std::string node_name, cvedix_stream_info stream_info) {
            std::lock_guard<std::mutex> guard(stream_info_hooker_lock);
            if (this->stream_info_hooker) {
                this->stream_info_hooker(node_name, stream_info);
            }
        }
    };
}