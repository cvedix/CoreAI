#pragma once

#include "cvedix_msg_broker_node.h"
#include "cvedix/third_party/cpp_httplib/httplib.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"
#include "cvedix/third_party/asio/include/asio.hpp"
#include <atomic>
#include <memory>

namespace cvedix_nodes
{
    class cvedix_sse_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief JSON transformer function
        std::function<std::string(const std::string&)> json_transformer = nullptr;
        /// @brief Endpoint
        std::string endpoint;
        /// @brief IP
        std::string ip="0.0.0.0";
        /// @brief Port
        int port;
        /// @brief IO context
        asio::io_context io;
        /// @brief Acceptor
        std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
        /// @brief Server thread
        std::thread server_thread;
        /// @brief Running flag
        std::atomic_bool running{true};
        /// @brief Client sockets mutex
        std::mutex client_sockets_mutex;
        /// @brief Client sockets
        std::vector<std::shared_ptr<asio::ip::tcp::socket>> client_sockets;

        /// @brief Start server
        void start();
        /// @brief Stop server
        void stop();
        /// @brief Send headers
        /// @param socket Socket to send headers
        void send_headers(std::shared_ptr<asio::ip::tcp::socket> socket);
        /// @brief Accept filter
        void accept_filter(std::shared_ptr<asio::ip::tcp::socket> socket);
        /// @brief Accept new client 
        void accept();        

    protected:
        /**
         * @brief Serialize to JSON
         * @param meta Frame meta
         * @param[out] msg Output JSON
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        
        /**
         * @brief Publish via user callback
         * @param msg JSON message
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Node name
         * @param broke_for Broke for
         * @param broking_cache_warn_threshold Broking cache warn threshold
         * @param broking_cache_ignore_threshold Broking cache ignore threshold
         * @param port Port
         * @param endpoint Endpoint
         */
        cvedix_sse_broker_node(
            std::string node_name,
            cvedix_broke_for broke_for = cvedix_broke_for::NORMAL,
            int broking_cache_warn_threshold = 50,
            int broking_cache_ignore_threshold = 200,
            int port = 8090,
            std::string endpoint = "/events");
        
        ~cvedix_sse_broker_node();

        /**
         * @brief Set JSON transformer
         * @param transformer JSON transformer function
         */
        void set_json_transformer(std::function<std::string(const std::string&)> transformer);
    };
    
} // namespace cvedix_nodes
