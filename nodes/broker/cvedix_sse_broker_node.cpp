#include "cvedix_sse_broker_node.h"
#include <istream>
#include <memory>
#include <sstream>

namespace cvedix_nodes
{
    cvedix_sse_broker_node::cvedix_sse_broker_node(
        std::string node_name,
        cvedix_broke_for broke_for,
        int broking_cache_warn_threshold,
        int broking_cache_ignore_threshold,
        int port,
        std::string endpoint):
        cvedix_msg_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold),
        port(port),
        endpoint(endpoint)
    {
        this->initialized();
        // Start server
        start();
    }

    cvedix_sse_broker_node::~cvedix_sse_broker_node()
    {
        // Stop listening server
        stop();
        deinitialized();
        stop_broking();
    }

    void cvedix_sse_broker_node::start()
    {
        // Create acceptor
        acceptor = std::make_unique<asio::ip::tcp::acceptor>(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
        // Register accept event to io context
        accept();
        // Start server thread
        server_thread = std::thread([this]() {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Server listen at ip = %s, port = %d", node_name.c_str(), ip.c_str(), port));
            // Run io context
            io.run();
        });
    }

    void cvedix_sse_broker_node::stop()
    {
        CVEDIX_INFO(cvedix_utils::string_format("[%s] Server stopped", node_name.c_str()));
        // Set running flag to false
        running = false;
        // Close acceptor
        acceptor->close();
        // Clear client sockets
        {
            std::lock_guard<std::mutex> lock(client_sockets_mutex);
            client_sockets.clear();
        }
        // Stop io context
        io.stop();
        server_thread.join();
    }

    void cvedix_sse_broker_node::send_headers(std::shared_ptr<asio::ip::tcp::socket> socket)
    {
        static const std::string headers =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n\r\n";

            asio::async_write(
                *socket,
                asio::buffer(headers),
                [this, socket](asio::error_code ec, std::size_t) 
                {
                    if (ec) CVEDIX_ERROR(cvedix_utils::string_format("[%s] Cannot send header because: %s", node_name.c_str(), ec.message()));
                }
            );
    }

    void cvedix_sse_broker_node::accept_filter(std::shared_ptr<asio::ip::tcp::socket> socket)
    {
        auto buf = std::make_shared<asio::streambuf>();
        asio::async_read_until(
            *socket,
            *buf,
            "\r\n\r\n",
            [this, buf, socket](asio::error_code ec, std::size_t) mutable
            {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Filter is checking", node_name.c_str()));

                if (ec) {
                    CVEDIX_ERROR(cvedix_utils::string_format("[%s] Filter not accept this request: %s", node_name.c_str(), ec.message()));
                    return;
                }

                std::istream is(buf.get());
                std::string request_line;
                std::getline(is, request_line);
                if (!request_line.empty() && request_line.back() == '\r')
                    request_line.pop_back();

                std::istringstream iss(request_line);
                std::string method, path, version;
                iss >> method >> path >> version;

                if (method == "GET" && path == endpoint)
                {
                    send_headers(socket);
                    // Add client socket to list
                    {
                        std::lock_guard<std::mutex> lock(client_sockets_mutex);
                        client_sockets.push_back(socket);
                        CVEDIX_INFO(cvedix_utils::string_format("[%s] Client connected, total=%d", node_name.c_str(), client_sockets.size()));
                    }
                }
                else CVEDIX_INFO(cvedix_utils::string_format("[%s] Client be filtered", node_name.c_str()));
            }
        );
    }
    
    void cvedix_sse_broker_node::accept()
    {
        if (!running) return;

        auto socket = std::make_shared<asio::ip::tcp::socket>(io);

        acceptor->async_accept(
            *socket,
            [this, socket](asio::error_code ec)
            {
                if (!ec && running) 
                {
                    accept_filter(socket);
                }
                else {
                    CVEDIX_ERROR(cvedix_utils::string_format("[%s] Accept failed: %s", node_name.c_str(), ec.message()));
                    return;
                }
                // Accept new client
                accept();
            }
        );
    }

    void cvedix_sse_broker_node::format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg)
    {
        // Serialize objects to JSON by cereal (same as json_console_broker_node)
        try {
            std::stringstream msg_stream;
            {
                cereal::JSONOutputArchive json_archive(msg_stream);
                
                // Global values
                json_archive(cereal::make_nvp("channel_index", meta->channel_index),
                            cereal::make_nvp("frame_index", meta->frame_index),
                            cereal::make_nvp("width", meta->frame.cols),
                            cereal::make_nvp("height", meta->frame.rows),
                            cereal::make_nvp("fps", meta->fps),
                            cereal::make_nvp("broke_for", broke_fors.at(broke_for)));

                // Serialize values according to broke_for
                if (broke_for == cvedix_broke_for::NORMAL) {
                    json_archive(cereal::make_nvp("target_size", meta->targets.size()), 
                                cereal::make_nvp("targets", meta->targets));
                }
                else if (broke_for == cvedix_broke_for::FACE) {
                    json_archive(cereal::make_nvp("face_target_size", meta->face_targets.size()),
                                cereal::make_nvp("face_targets", meta->face_targets));
                }
                else if (broke_for == cvedix_broke_for::TEXT) {
                    json_archive(cereal::make_nvp("text_target_size", meta->text_targets.size()),
                                cereal::make_nvp("text_targets", meta->text_targets));
                }
                else {
                    throw "invalid broke_for!";
                }
            } // flush
            
            msg = msg_stream.str();
            
            // Apply custom JSON transformation if provided
            if (json_transformer != nullptr) {
                try {
                    msg = json_transformer(msg);
                } catch (const std::exception& e) {
                    CVEDIX_ERROR(cvedix_utils::string_format("[%s] JSON transformation failed: %s", 
                        node_name.c_str(), e.what()));
                    // Continue with original message if transformation fails
                } catch (...) {
                    CVEDIX_ERROR(cvedix_utils::string_format("[%s] JSON transformation failed with unknown error", 
                        node_name.c_str()));
                    // Continue with original message if transformation fails
                }
            }
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] format_msg failed: %s", 
                node_name.c_str(), e.what()));
            msg = "";
        } catch (...) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] format_msg failed with unknown error", 
                node_name.c_str()));
            msg = "";
        }
    }


    void cvedix_sse_broker_node::broke_msg(const std::string& msg)
    {
        if (!running) return;
        asio::post(io,
            [this, msg]()
            {
                // Create send message
                auto send_msg = std::make_shared<std::string>(msg + "\n\n");

                for (auto it = client_sockets.begin(); it != client_sockets.end();)
                {
                    if (!(*it)->is_open()) {
                        // Remove closed socket from client_sockets
                        {
                            std::lock_guard<std::mutex> lock(client_sockets_mutex);
                            CVEDIX_INFO(cvedix_utils::string_format("[%s] Removing closed socket from client_sockets", node_name.c_str()));
                            it = client_sockets.erase(it);
                        }
                        continue;
                    }

                    asio::async_write(
                        **it,
                        asio::buffer(*send_msg),
                        [this, socket = *it, send_msg](asio::error_code ec, std::size_t)
                        {
                            if (ec) {
                                CVEDIX_ERROR(cvedix_utils::string_format("[%s] Cannot broke because: %s", node_name.c_str(), ec.message()));
                                socket->close(); // cleanup after broadcast
                            }
                        }
                    );
                    // Increment iterator
                    ++it;
                }
            }
        );
    }

    void cvedix_sse_broker_node::set_json_transformer(std::function<std::string(const std::string&)> transformer)
    {
        json_transformer = transformer;
    }

} // namespace cvedix_nodes


