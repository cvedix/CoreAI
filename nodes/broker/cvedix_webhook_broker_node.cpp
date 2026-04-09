#include "cvedix_webhook_broker_node.h"

#include <iostream>
#include <sstream>
#include <regex>

namespace cvedix_nodes {

// ─── URL parser ─────────────────────────────────────────────────
// Splits "http://host:port/path" into parsed_host="http://host:port"
// and parsed_path="/path".

void cvedix_webhook_broker_node::parse_url() {
    // Match: scheme://host[:port][/path]
    std::regex url_regex(R"(^(https?://[^/]+)(/.*)?)");
    std::smatch match;

    if (std::regex_match(webhook_url, match, url_regex)) {
        parsed_host = match[1].str();       // "http://host:port"
        parsed_path = match[2].str();       // "/path/to/endpoint"
        if (parsed_path.empty()) {
            parsed_path = "/";
        }
    } else {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] Invalid webhook URL: %s", node_name.c_str(), webhook_url.c_str()));
        parsed_host = "";
        parsed_path = "/";
    }
}

// ─── Constructor ────────────────────────────────────────────────

cvedix_webhook_broker_node::cvedix_webhook_broker_node(
    std::string node_name,
    std::string webhook_url,
    cvedix_broke_for broke_for,
    int broking_cache_warn_threshold,
    int broking_cache_ignore_threshold,
    std::function<std::string(const std::string&)> json_transformer,
    httplib::Headers custom_headers,
    int timeout_sec,
    int retry_count)
    : cvedix_msg_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold),
      webhook_url(webhook_url),
      json_transformer(json_transformer),
      custom_headers(custom_headers),
      timeout_sec(timeout_sec),
      retry_count(retry_count) {

    // Parse URL into host + path components
    parse_url();

    // Start the node's handle/dispatch threads
    this->initialized();

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Webhook broker initialized: url=%s (host=%s, path=%s, timeout=%ds, retries=%d)",
        node_name.c_str(), webhook_url.c_str(),
        parsed_host.c_str(), parsed_path.c_str(),
        timeout_sec, retry_count));

    if (parsed_host.empty()) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Webhook URL is invalid — messages will be logged but not sent.",
            node_name.c_str()));
    }
}

// ─── Destructor ─────────────────────────────────────────────────

cvedix_webhook_broker_node::~cvedix_webhook_broker_node() {
    deinitialized();
    stop_broking();
}

// ─── Setters ────────────────────────────────────────────────────

void cvedix_webhook_broker_node::set_webhook_url(const std::string& url) {
    webhook_url = url;
    parse_url();
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Webhook URL updated: %s (host=%s, path=%s)",
        node_name.c_str(), url.c_str(), parsed_host.c_str(), parsed_path.c_str()));
}

std::string cvedix_webhook_broker_node::get_webhook_url() const {
    return webhook_url;
}

void cvedix_webhook_broker_node::set_json_transformer(
    std::function<std::string(const std::string&)> transformer) {
    json_transformer = transformer;
}

void cvedix_webhook_broker_node::set_custom_headers(httplib::Headers headers) {
    custom_headers = std::move(headers);
}

void cvedix_webhook_broker_node::add_custom_header(
    const std::string& key, const std::string& value) {
    custom_headers.insert({key, value});
}

void cvedix_webhook_broker_node::set_timeout(int seconds) {
    timeout_sec = seconds;
}

void cvedix_webhook_broker_node::set_retry_count(int count) {
    retry_count = count;
}

// ─── format_msg: serialize frame_meta to JSON ──────────────────

void cvedix_webhook_broker_node::format_msg(
    const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta,
    std::string& msg) {

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
                throw std::runtime_error("invalid broke_for!");
            }
        } // flush

        msg = msg_stream.str();

        // Apply custom JSON transformation if provided
        if (json_transformer != nullptr) {
            try {
                msg = json_transformer(msg);
            } catch (const std::exception& e) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] JSON transformation failed: %s",
                    node_name.c_str(), e.what()));
            } catch (...) {
                CVEDIX_ERROR(cvedix_utils::string_format(
                    "[%s] JSON transformation failed with unknown error",
                    node_name.c_str()));
            }
        }
    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] format_msg failed: %s", node_name.c_str(), e.what()));
        msg = "";
    } catch (...) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] format_msg failed with unknown error", node_name.c_str()));
        msg = "";
    }
}

// ─── broke_msg: HTTP POST to webhook ────────────────────────────

void cvedix_webhook_broker_node::broke_msg(const std::string& msg) {
    if (msg.empty()) return;

    if (parsed_host.empty()) {
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] No valid webhook URL, message: %s",
            node_name.c_str(), msg.substr(0, 200).c_str()));
        return;
    }

    int attempts = 1 + retry_count;  // 1 initial + retries

    for (int attempt = 1; attempt <= attempts; ++attempt) {
        try {
            // Create a fresh client per POST (simple, thread-safe)
            httplib::Client cli(parsed_host);
            cli.set_connection_timeout(timeout_sec, 0);
            cli.set_read_timeout(timeout_sec, 0);
            cli.set_write_timeout(timeout_sec, 0);

            // Build headers: Content-Type + custom headers
            httplib::Headers headers = custom_headers;
            // httplib::Client::Post() sets Content-Type via the parameter,
            // but we also merge custom_headers in case user wants to override.

            auto res = cli.Post(parsed_path, headers, msg, "application/json");

            if (res) {
                if (res->status >= 200 && res->status < 300) {
                    // Success
                    return;
                } else {
                    CVEDIX_WARN(cvedix_utils::string_format(
                        "[%s] Webhook returned HTTP %d (attempt %d/%d): %s",
                        node_name.c_str(), res->status, attempt, attempts,
                        webhook_url.c_str()));
                }
            } else {
                auto err = res.error();
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] Webhook POST failed (attempt %d/%d): %s → %s",
                    node_name.c_str(), attempt, attempts,
                    httplib::to_string(err).c_str(), webhook_url.c_str()));
            }
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Webhook POST exception (attempt %d/%d): %s",
                node_name.c_str(), attempt, attempts, e.what()));
        } catch (...) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Webhook POST unknown exception (attempt %d/%d)",
                node_name.c_str(), attempt, attempts));
        }
    }

    // All attempts exhausted
    CVEDIX_ERROR(cvedix_utils::string_format(
        "[%s] Webhook delivery failed after %d attempts: %s",
        node_name.c_str(), attempts, webhook_url.c_str()));
}

} // namespace cvedix_nodes
