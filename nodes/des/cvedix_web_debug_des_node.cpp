#include "cvedix_web_debug_des_node.h"
#include "web_debug_dashboard.h"

#include <sstream>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace cvedix_nodes {

    bool cvedix_web_debug_des_node::acquire_instance_lock() {
        instance_lock_path = "/tmp/cvedix_web_debug_port_" + std::to_string(port) + ".lock";
        instance_lock_fd = ::open(instance_lock_path.c_str(), O_CREAT | O_RDWR, 0666);
        if (instance_lock_fd < 0) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Failed to open lock file %s: %s",
                node_name.c_str(),
                instance_lock_path.c_str(),
                std::strerror(errno)));
            return false;
        }

        if (::flock(instance_lock_fd, LOCK_EX | LOCK_NB) != 0) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Port %d is already owned by another web debug instance. Stop the old sample or use --port <new_port>.",
                node_name.c_str(),
                port));
            ::close(instance_lock_fd);
            instance_lock_fd = -1;
            return false;
        }

        const std::string pid_line = std::to_string(::getpid()) + "\n";
        ::ftruncate(instance_lock_fd, 0);
        ::lseek(instance_lock_fd, 0, SEEK_SET);
        (void)::write(instance_lock_fd, pid_line.c_str(), pid_line.size());
        return true;
    }

    void cvedix_web_debug_des_node::release_instance_lock() {
        if (instance_lock_fd < 0) {
            return;
        }

        ::flock(instance_lock_fd, LOCK_UN);
        ::close(instance_lock_fd);
        instance_lock_fd = -1;
    }

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
        stats.last_sse_publish_time = stats.start_time - std::chrono::milliseconds(kSseThrottleMs);
        latest_board_snapshot_time = stats.start_time - std::chrono::milliseconds(kBoardSnapshotCacheMs);

        if (!acquire_instance_lock()) {
            throw std::runtime_error(cvedix_utils::string_format(
                "web debug port %d is already active in another sample instance",
                this->port));
        }

        // Enable concurrent request handling (CRITICAL for MJPEG + SSE)
        // Without this, the first MJPEG stream blocks all other endpoints
        server.new_task_queue = [] { return new httplib::ThreadPool(8); };

        setup_routes();

        server_thread = std::thread([this]() {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Web debug dashboard at http://0.0.0.0:%d", this->node_name.c_str(), this->port));
            const bool listen_ok = server.listen("0.0.0.0", this->port);
            if (!listen_ok && running) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] Web debug server stopped unexpectedly on port %d",
                    this->node_name.c_str(),
                    this->port));
            }
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
        release_instance_lock();
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

    std::vector<uint8_t> cvedix_web_debug_des_node::encode_jpeg(const cv::Mat& frame, int max_width, int quality) {
        std::vector<uint8_t> buf;
        const int effective_quality = quality > 0 ? quality : jpeg_quality;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, effective_quality};

        cv::Mat resized_frame;
        if (frame.cols > max_width) {
            float ratio = static_cast<float>(max_width) / frame.cols;
            cv::resize(frame, resized_frame, cv::Size(), ratio, ratio);
        } else {
            resized_frame = frame;
        }

        cv::imencode(".jpg", resized_frame, buf, params);
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

        const cv::Mat& output_frame = meta->osd_frame.empty() ? meta->frame : meta->osd_frame;

        // Update latest frame (thread-safe swap)
        {
            std::lock_guard<std::mutex> guard(frame_lock);
            latest_frame = output_frame;
            latest_orig_frame = meta->frame;
            latest_frame_seq++;
        }

        const auto target_count = meta->targets.size() + meta->face_targets.size();
        const auto now_system = std::chrono::system_clock::now();
        const auto now_steady = std::chrono::steady_clock::now();
        const auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now_system - meta->create_time).count();
        bool should_publish_sse = false;

        // Update stats
        {
            std::lock_guard<std::mutex> guard(stats_lock);
            stats.frame_count++;
            stats.latency_ms = static_cast<int>(latency_ms);
            stats.object_count = static_cast<int>(target_count);
            stats.queue_size = in_queue.size();

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - stats.last_fps_time).count();
            if (elapsed >= 1000) {
                stats.fps = stats.frame_count * 1000.0 / elapsed;
                stats.frame_count = 0;
                stats.last_fps_time = now;
            }

            if (target_count > 0) {
                auto since_last_sse = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now_steady - stats.last_sse_publish_time).count();
                if (since_last_sse >= kSseThrottleMs) {
                    stats.last_sse_publish_time = now_steady;
                    should_publish_sse = true;
                }
            }
        }

        if (should_publish_sse) {
            std::string json = "{\"channel\":" + std::to_string(meta->channel_index)
                + ",\"targets_count\":" + std::to_string(target_count)
                + ",\"timestamp\":" + std::to_string(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count())
                + "}";
            broadcast_sse(json);
        }

        return cvedix_des_node::handle_frame_meta(meta); // terminal node + board status hook
    }

    void cvedix_web_debug_des_node::setup_routes() {
        // Dashboard page
        server.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
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
                    uint64_t last_encoded_seq = 0;
                    while (running) {
                        cv::Mat frame;
                        uint64_t current_seq = 0;
                        {
                            std::lock_guard<std::mutex> guard(frame_lock);
                            current_seq = latest_frame_seq;
                            if (current_seq > last_encoded_seq && !latest_frame.empty()) {
                                frame = latest_frame;
                            }
                        }
                        if (!frame.empty()) {
                            auto jpg = encode_jpeg(frame);
                            std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                                + std::to_string(jpg.size()) + "\r\n\r\n";
                            sink.write(header.c_str(), header.size());
                            sink.write(reinterpret_cast<const char*>(jpg.data()), jpg.size());
                            sink.write("\r\n", 2);
                            last_encoded_seq = current_seq;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps
                    }
                    return false;
                });
        });

        // MJPEG stream — Original video
        server.Get("/stream/orig", [this](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content_provider(
                "multipart/x-mixed-replace; boundary=frame",
                [this](size_t /*offset*/, httplib::DataSink& sink) {
                    uint64_t last_encoded_seq = 0;
                    while (running) {
                        cv::Mat frame;
                        uint64_t current_seq = 0;
                        {
                            std::lock_guard<std::mutex> guard(frame_lock);
                            current_seq = latest_frame_seq;
                            if (current_seq > last_encoded_seq && !latest_orig_frame.empty()) {
                                frame = latest_orig_frame;
                            }
                        }
                        if (!frame.empty()) {
                            auto jpg = encode_jpeg(frame);
                            std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                                + std::to_string(jpg.size()) + "\r\n\r\n";
                            sink.write(header.c_str(), header.size());
                            sink.write(reinterpret_cast<const char*>(jpg.data()), jpg.size());
                            sink.write("\r\n", 2);
                            last_encoded_seq = current_seq;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps
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
                        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10fps for board
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

        // Snapshot — single JPEG frame (cross-browser compatible fallback)
        server.Get("/snapshot/osd", [this](const httplib::Request&, httplib::Response& res) {
            cv::Mat frame;
            uint64_t frame_seq = 0;
            {
                std::lock_guard<std::mutex> guard(frame_lock);
                if (!latest_frame.empty()) {
                    frame = latest_frame;
                    frame_seq = latest_frame_seq;
                }
            }
            if (!frame.empty()) {
                std::vector<uint8_t> jpg;
                {
                    std::lock_guard<std::mutex> guard(snapshot_lock);
                    if (latest_osd_snapshot.bytes.empty() || latest_osd_snapshot.source_seq != frame_seq) {
                        latest_osd_snapshot.bytes = encode_jpeg(frame, kSnapshotMaxWidth, kSnapshotJpegQuality);
                        latest_osd_snapshot.source_seq = frame_seq;
                    }
                    jpg = latest_osd_snapshot.bytes;
                }
                res.set_header("Cache-Control", "no-cache, no-store");
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(std::string(reinterpret_cast<const char*>(jpg.data()), jpg.size()), "image/jpeg");
            } else {
                res.status = 204;
            }
        });

        server.Get("/snapshot/orig", [this](const httplib::Request&, httplib::Response& res) {
            cv::Mat frame;
            uint64_t frame_seq = 0;
            {
                std::lock_guard<std::mutex> guard(frame_lock);
                if (!latest_orig_frame.empty()) {
                    frame = latest_orig_frame;
                    frame_seq = latest_frame_seq;
                }
            }
            if (!frame.empty()) {
                std::vector<uint8_t> jpg;
                {
                    std::lock_guard<std::mutex> guard(snapshot_lock);
                    if (latest_orig_snapshot.bytes.empty() || latest_orig_snapshot.source_seq != frame_seq) {
                        latest_orig_snapshot.bytes = encode_jpeg(frame, kSnapshotMaxWidth, kSnapshotJpegQuality);
                        latest_orig_snapshot.source_seq = frame_seq;
                    }
                    jpg = latest_orig_snapshot.bytes;
                }
                res.set_header("Cache-Control", "no-cache, no-store");
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(std::string(reinterpret_cast<const char*>(jpg.data()), jpg.size()), "image/jpeg");
            } else {
                res.status = 204;
            }
        });

        server.Get("/snapshot/board", [this](const httplib::Request&, httplib::Response& res) {
            std::vector<uint8_t> png;
            const auto now = std::chrono::steady_clock::now();

            {
                std::lock_guard<std::mutex> guard(snapshot_lock);
                auto cache_age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - latest_board_snapshot_time).count();
                if (!latest_board_snapshot.empty() && cache_age < kBoardSnapshotCacheMs) {
                    png = latest_board_snapshot;
                }
            }

            if (png.empty() && board) {
                cv::Mat canvas = board->get_current_canvas();
                if (!canvas.empty()) {
                    cv::Mat resized_canvas;
                    if (canvas.cols > kBoardSnapshotMaxWidth) {
                        float ratio = static_cast<float>(kBoardSnapshotMaxWidth) / canvas.cols;
                        cv::resize(canvas, resized_canvas, cv::Size(), ratio, ratio);
                    } else {
                        resized_canvas = canvas;
                    }

                    std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 3};
                    cv::imencode(".png", resized_canvas, png, params);

                    std::lock_guard<std::mutex> guard(snapshot_lock);
                    latest_board_snapshot = png;
                    latest_board_snapshot_time = now;
                }
            }

            if (!png.empty()) {
                res.set_header("Cache-Control", "no-cache, no-store");
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content(std::string(reinterpret_cast<const char*>(png.data()), png.size()), "image/png");
                return;
            }

            res.status = 204;
        });
    }

} // namespace cvedix_nodes
