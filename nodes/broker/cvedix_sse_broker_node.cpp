#include "cvedix_sse_broker_node.h"
#include <iostream>
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
        start();
    }

    cvedix_sse_broker_node::~cvedix_sse_broker_node()
    {
        // Stop listening server
        stop();
        // Note: deinitialized() and stop_broking() are already called by base class
        // Do not call them again here
    }

    void cvedix_sse_broker_node::start()
    {
        if (running) return;
        running = true;

        // ---------- SSE ENDPOINT ----------
        server.Get(endpoint,[this](const httplib::Request&, httplib::Response& res)
        {
            res.set_header("Content-Type", "text/event-stream");
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");

            res.set_chunked_content_provider(
                "text/event-stream",
                [this](size_t, httplib::DataSink& sink)
                {
                    {
                        std::lock_guard<std::mutex> lk(clients_mtx);
                        clients.push_back(&sink);
                        CVEDIX_INFO(cvedix_utils::string_format("[%s] Client connected, total = %d", node_name.c_str(), clients.size()));
                    }

                    // keep connection alive
                    while (running) {
                        std::this_thread::sleep_for(
                            std::chrono::seconds(3));
                    }

                    return true;
                }
            );
        });

        // ---------- SERVER THREAD ----------
        server_thread = std::thread([this]() {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Server listen at ip = %s, port = %d", node_name.c_str(), ip, port));
            server.listen(ip.c_str(), port);
        });
    }

    void cvedix_sse_broker_node::stop()
    {
        if (!running) return;
        running = false;

        CVEDIX_INFO(cvedix_utils::string_format("[%s] Server stop", node_name.c_str()));
        server.stop();

        if (server_thread.joinable())
            server_thread.join();

        std::lock_guard<std::mutex> lk(clients_mtx);
        clients.clear();
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
        std::lock_guard<std::mutex> lk(clients_mtx);

        for (auto it = clients.begin(); it != clients.end(); )
        {
            if (!(*it)->write(msg.c_str(), msg.size())) {
                CVEDIX_INFO(cvedix_utils::string_format("[%s] Client disconnected", node_name.c_str()));
                it = clients.erase(it);
            } else {
                ++it;
            }
        }
    }

    void cvedix_sse_broker_node::set_json_transformer(std::function<std::string(const std::string&)> transformer)
    {
        json_transformer = transformer;
    }

} // namespace cvedix_nodes


