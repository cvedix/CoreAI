/**
 * @file cvedix_webhook_broker_node.h
 * @brief HTTP webhook broker for JSON detection/analytics results
 *
 * Transport-layer broker that serializes pipeline data to JSON and
 * HTTP POSTs it to a configurable webhook endpoint.
 * Uses cpp_httplib (header-only, already in third_party/) — no external deps.
 *
 * @section webhook_features Features
 * - HTTP POST JSON to any URL
 * - Custom headers support (e.g. Authorization, API keys)
 * - Optional JSON transformation function
 * - Configurable timeout and retry count
 * - Asynchronous (non-blocking pipeline)
 *
 * @section webhook_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_webhook_broker_node>(
 *     "webhook_broker",
 *     "http://my-server:8080/api/detections",
 *     cvedix_broke_for::NORMAL,
 *     50, 200,
 *     // Optional JSON transformer
 *     [](const std::string& json) {
 *         return "{\"source\":\"camera1\",\"data\":" + json + "}";
 *     },
 *     // Optional custom headers
 *     {{"Authorization", "Bearer my-token"}, {"X-Device-Id", "cam-01"}}
 * );
 * broker->attach_to({tracker_node});
 * @endcode
 *
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#include <sstream>
#include <functional>
#include <string>
#include <map>

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"
#include "cvedix/third_party/cpp_httplib/httplib.h"

namespace cvedix_nodes {

    /**
     * @brief HTTP Webhook JSON broker
     *
     * Serializes frame_meta to JSON and HTTP POSTs to a webhook URL.
     * Supports custom headers, JSON transformation, retry, and timeout.
     *
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_webhook_broker_node : public cvedix_msg_broker_node
    {
    private:
        /// @brief Target webhook URL (e.g. "http://host:port/path")
        std::string webhook_url;

        /// @brief Parsed host+port for httplib::Client (e.g. "http://host:port")
        std::string parsed_host;

        /// @brief Parsed path for POST request (e.g. "/api/events")
        std::string parsed_path;

        /// @brief Optional JSON transformation function (input → output)
        std::function<std::string(const std::string&)> json_transformer = nullptr;

        /// @brief Custom HTTP headers to include in every POST
        httplib::Headers custom_headers;

        /// @brief Connection + read timeout in seconds
        int timeout_sec = 5;

        /// @brief Number of retry attempts on failure (0 = no retry)
        int retry_count = 1;

        /// @brief Parse webhook_url into host and path components
        void parse_url();

    protected:
        /**
         * @brief Serialize frame meta to JSON via cereal
         * @param meta Frame meta
         * @param[out] msg Output JSON string
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief HTTP POST the JSON message to webhook URL
         * @param msg JSON message to POST
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param webhook_url Target URL for HTTP POST (e.g. "http://localhost:9999/events")
         * @param broke_for Target data type to serialize
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         * @param json_transformer Optional JSON transformation function
         * @param custom_headers Optional custom HTTP headers
         * @param timeout_sec Connection timeout in seconds (default: 5)
         * @param retry_count Number of retries on failure (default: 1)
         */
        cvedix_webhook_broker_node(
            std::string node_name,
            std::string webhook_url,
            cvedix_broke_for broke_for = cvedix_broke_for::NORMAL,
            int broking_cache_warn_threshold = 50,
            int broking_cache_ignore_threshold = 200,
            std::function<std::string(const std::string&)> json_transformer = nullptr,
            httplib::Headers custom_headers = {},
            int timeout_sec = 5,
            int retry_count = 1);

        /// @brief Destructor
        ~cvedix_webhook_broker_node();

        /**
         * @brief Update webhook URL at runtime
         * @param url New webhook URL
         */
        void set_webhook_url(const std::string& url);

        /**
         * @brief Get current webhook URL
         * @return Current URL
         */
        std::string get_webhook_url() const;

        /**
         * @brief Set JSON transformer
         * @param transformer Function: input JSON → transformed JSON
         */
        void set_json_transformer(std::function<std::string(const std::string&)> transformer);

        /**
         * @brief Set custom HTTP headers
         * @param headers Headers map (replaces existing)
         */
        void set_custom_headers(httplib::Headers headers);

        /**
         * @brief Add a single custom HTTP header
         * @param key Header name
         * @param value Header value
         */
        void add_custom_header(const std::string& key, const std::string& value);

        /**
         * @brief Set timeout for HTTP requests
         * @param seconds Timeout in seconds
         */
        void set_timeout(int seconds);

        /**
         * @brief Set retry count
         * @param count Number of retries (0 = no retry)
         */
        void set_retry_count(int count);

        /**
         * @brief Set max input queue size
         * @param size Queue size (increase for high FPS)
         */
        void set_max_queue_size(int size) { max_in_queue_size = size; }
    };

} // namespace cvedix_nodes
