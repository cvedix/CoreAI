/**
 * @file cvedix_web_debug_des_node.h
 * @brief Web-based debug destination node with MJPEG + SSE
 *
 * Serves a browser-based debug dashboard with:
 * - MJPEG stream of OSD-rendered video frames
 * - MJPEG stream of the analysis board
 * - SSE (Server-Sent Events) for real-time detection metadata
 * - JSON stats API
 *
 * No X11 required — works in any browser on any platform.
 *
 * @section usage Usage
 * @code
 * cvedix_analysis_board board({src});
 * board.push_to_buffer(5);
 *
 * auto web = std::make_shared<cvedix_web_debug_des_node>(
 *     "web_debug", 0, 9090, &board);
 * web->attach_to({osd});
 * // Open http://localhost:9090 in browser
 * @endcode
 */

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include "cvedix/nodes/common/cvedix_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/third_party/cpp_httplib/httplib.h"

namespace cvedix_nodes {

    /**
     * @brief Web debug destination node
     *
     * Replaces screen_des_node for remote debugging via browser.
     * Serves MJPEG video + analysis board + SSE events.
     */
    class cvedix_web_debug_des_node : public cvedix_des_node {
    private:
        /// @brief HTTP server
        httplib::Server server;

        /// @brief Server thread
        std::thread server_thread;

        /// @brief Port
        int port;

        /// @brief JPEG encode quality (0-100)
        int jpeg_quality;

        /// @brief Analysis board reference (optional)
        cvedix_utils::cvedix_analysis_board* board = nullptr;

        /// @brief Latest OSD frame (thread-safe)
        cv::Mat latest_frame;
        cv::Mat latest_orig_frame;
        uint64_t latest_frame_seq = 0;
        std::mutex frame_lock;

        struct encoded_snapshot_cache {
            std::vector<uint8_t> bytes;
            uint64_t source_seq = 0;
        };
        encoded_snapshot_cache latest_osd_snapshot;
        encoded_snapshot_cache latest_orig_snapshot;
        std::vector<uint8_t> latest_board_snapshot;
        std::chrono::steady_clock::time_point latest_board_snapshot_time;
        std::mutex snapshot_lock;

        /// @brief Latest frame meta for stats
        struct Stats {
            double fps = 0;
            int latency_ms = 0;
            int object_count = 0;
            int queue_size = 0;
            std::chrono::steady_clock::time_point start_time;
            int frame_count = 0;
            std::chrono::steady_clock::time_point last_fps_time;
            std::chrono::steady_clock::time_point last_sse_publish_time;
        } stats;
        std::mutex stats_lock;

        /// @brief SSE client connections
        struct SSEClient {
            httplib::DataSink* sink;
            bool alive;
        };
        std::vector<std::shared_ptr<SSEClient>> sse_clients;
        std::mutex sse_lock;

        /// @brief Running flag
        std::atomic_bool running{true};

        /// @brief Cross-process lock to prevent duplicate dashboard instances on one port
        int instance_lock_fd = -1;
        std::string instance_lock_path;

        static constexpr int kSnapshotMaxWidth = 640;
        static constexpr int kBoardSnapshotMaxWidth = 1100;
        static constexpr int kSnapshotJpegQuality = 60;
        static constexpr int kSseThrottleMs = 250;
        static constexpr int kBoardSnapshotCacheMs = 800;

        bool acquire_instance_lock();
        void release_instance_lock();

        /// @brief Setup HTTP routes
        void setup_routes();

        /// @brief Encode frame to JPEG bytes
        std::vector<uint8_t> encode_jpeg(const cv::Mat& frame, int max_width = kSnapshotMaxWidth, int quality = -1);

        /// @brief Send MJPEG stream
        void stream_mjpeg(httplib::Response& res, bool is_board);

        /// @brief Broadcast SSE message to all connected clients
        void broadcast_sse(const std::string& msg);

    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node name
         * @param channel_index Channel index
         * @param port HTTP port (default: 9090)
         * @param board Analysis board pointer (optional, for board stream)
         * @param jpeg_quality JPEG encode quality 0-100 (default: 75)
         */
        cvedix_web_debug_des_node(
            std::string node_name,
            int channel_index = 0,
            int port = 9090,
            cvedix_utils::cvedix_analysis_board* board = nullptr,
            int jpeg_quality = 75);

        ~cvedix_web_debug_des_node();

        /** @brief Set analysis board pointer (for board stream) */
        void set_board(cvedix_utils::cvedix_analysis_board* board) { this->board = board; }

        /** @brief Get internal HTTP server to add custom routes */
        httplib::Server& get_server() { return server; }

        virtual std::string to_string() override;
    };

} // namespace cvedix_nodes
