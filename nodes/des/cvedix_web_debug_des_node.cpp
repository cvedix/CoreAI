#include "cvedix_web_debug_des_node.h"
#include "web_debug_dashboard.h"

#include <sstream>

namespace cvedix_nodes {

    cvedix_web_debug_des_node::cvedix_web_debug_des_node(
        std::string node_name,
        int channel_index,
        int port,
        cvedix_utils::cvedix_analysis_board* board,
        int jpeg_quality)
        : cvedix_des_node(node_name, channel_index),
          port(port),
          board(board),
          jpeg_quality(jpeg_quality) {

        stats.start_time = std::chrono::steady_clock::now();
        stats.last_fps_time = stats.start_time;

        setup_routes();

        server_thread = std::thread([this]() {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Web debug dashboard at http://0.0.0.0:%d", this->node_name.c_str(), this->port));
            server.listen("0.0.0.0", this->port);
        });

        initialized();
    }

    cvedix_web_debug_des_node::~cvedix_web_debug_des_node() {
        deinitialized();
        running = false;
        server.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
        // Close all SSE clients
        std::lock_guard<std::mutex> guard(sse_lock);
        for (auto& client : sse_clients) {
            client->alive = false;
        }
    }

    std::string cvedix_web_debug_des_node::to_string() {
        return "web_debug_des | port: " + std::to_string(port) +
               " | board: " + std::string(board ? "yes" : "no");
    }

    std::vector<uint8_t> cvedix_web_debug_des_node::encode_jpeg(const cv::Mat& frame) {
        std::vector<uint8_t> buf;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, jpeg_quality};
        cv::imencode(".jpg", frame, buf, params);
        return buf;
    }

    void cvedix_web_debug_des_node::broadcast_sse(const std::string& msg) {
        std::lock_guard<std::mutex> guard(sse_lock);
        std::string sse_data = "data: " + msg + "\n\n";
        auto it = sse_clients.begin();
        while (it != sse_clients.end()) {
            if (!(*it)->alive) {
                it = sse_clients.erase(it);
                continue;
            }
            try {
                (*it)->sink->write(sse_data.c_str(), sse_data.size());
            } catch (...) {
                (*it)->alive = false;
                it = sse_clients.erase(it);
                continue;
            }
            ++it;
        }
    }

    std::shared_ptr<cvedix_objects::cvedix_meta>
    cvedix_web_debug_des_node::handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

        if (!meta || meta->frame.empty()) return nullptr;

        // Update latest frame (thread-safe swap)
        {
            std::lock_guard<std::mutex> guard(frame_lock);
            latest_frame = meta->frame.clone();
        }

        // Update stats
        {
            std::lock_guard<std::mutex> guard(stats_lock);
            stats.frame_count++;
            stats.object_count = meta->targets.size();
            stats.queue_size = in_queue.size();

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - stats.last_fps_time).count();
            if (elapsed >= 1000) {
                stats.fps = stats.frame_count * 1000.0 / elapsed;
                stats.frame_count = 0;
                stats.last_fps_time = now;
            }
        }

        // Broadcast SSE event (simple JSON)
        if (!meta->targets.empty()) {
            std::string json = "{\"channel\":" + std::to_string(meta->channel_index)
                + ",\"targets_count\":" + std::to_string(meta->targets.size())
                + ",\"timestamp\":" + std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count())
                + "}";
            broadcast_sse(json);
        }

        return nullptr; // terminal node
    }

    void cvedix_web_debug_des_node::setup_routes() {
        // Dashboard page
        server.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(WEB_DEBUG_DASHBOARD_HTML, "text/html");
        });

        // MJPEG stream — OSD video
        server.Get("/stream/osd", [this](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content_provider(
                "multipart/x-mixed-replace; boundary=frame",
                [this](size_t /*offset*/, httplib::DataSink& sink) {
                    while (running) {
                        cv::Mat frame;
                        {
                            std::lock_guard<std::mutex> guard(frame_lock);
                            if (!latest_frame.empty()) {
                                frame = latest_frame.clone();
                            }
                        }
                        if (!frame.empty()) {
                            auto jpg = encode_jpeg(frame);
                            std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                                + std::to_string(jpg.size()) + "\r\n\r\n";
                            sink.write(header.c_str(), header.size());
                            sink.write(reinterpret_cast<const char*>(jpg.data()), jpg.size());
                            sink.write("\r\n", 2);
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps cap
                    }
                    return false;
                });
        });

        // MJPEG stream — Analysis Board
        server.Get("/stream/board", [this](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content_provider(
                "multipart/x-mixed-replace; boundary=frame",
                [this](size_t /*offset*/, httplib::DataSink& sink) {
                    while (running) {
                        if (board) {
                            cv::Mat canvas = board->get_current_canvas();
                            if (!canvas.empty()) {
                                auto jpg = encode_jpeg(canvas);
                                std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                                    + std::to_string(jpg.size()) + "\r\n\r\n";
                                sink.write(header.c_str(), header.size());
                                sink.write(reinterpret_cast<const char*>(jpg.data()), jpg.size());
                                sink.write("\r\n", 2);
                            }
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 5fps for board
                    }
                    return false;
                });
        });

        // SSE endpoint
        server.Get("/events", [this](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content_provider(
                "text/event-stream",
                [this](size_t /*offset*/, httplib::DataSink& sink) {
                    auto client = std::make_shared<SSEClient>();
                    client->sink = &sink;
                    client->alive = true;
                    {
                        std::lock_guard<std::mutex> guard(sse_lock);
                        sse_clients.push_back(client);
                    }
                    // Keep connection alive
                    while (running && client->alive) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    return false;
                });
        });

        // Stats API
        server.Get("/api/stats", [this](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> guard(stats_lock);
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - stats.start_time).count();

            std::string json = "{";
            json += "\"fps\":" + std::to_string(stats.fps) + ",";
            json += "\"latency_ms\":" + std::to_string(stats.latency_ms) + ",";
            json += "\"object_count\":" + std::to_string(stats.object_count) + ",";
            json += "\"queue_size\":" + std::to_string(stats.queue_size) + ",";
            json += "\"uptime_sec\":" + std::to_string(uptime);
            json += "}";

            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content(json, "application/json");
        });
    }

} // namespace cvedix_nodes
