#pragma once

#include "cvedix_msg_broker_node.h"
#include "cvedix/third_party/cpp_httplib/httplib.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"
// #include <third_party/asio/include/asio.hpp>


namespace cvedix_nodes
{
    class cvedix_sse_broker_node: public cvedix_msg_broker_node
    {
    private:
        
        std::function<std::string(const std::string&)> json_transformer = nullptr;

        httplib::Server server;

        std::string endpoint;

        std::string ip="0.0.0.0";

        int port;

        std::vector<httplib::DataSink*> clients;

        std::mutex clients_mtx;

        std::thread server_thread;

        std::atomic<bool> running{false};

        void start();

        void stop();

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
        cvedix_sse_broker_node(
            std::string node_name,
            cvedix_broke_for broke_for = cvedix_broke_for::NORMAL,
            int broking_cache_warn_threshold = 50,
            int broking_cache_ignore_threshold = 200,
            int port = 8090,
            std::string endpoint = "/events");
        
        ~cvedix_sse_broker_node();

        void set_json_transformer(std::function<std::string(const std::string&)> transformer);
    };
    
} // namespace cvedix_nodes
