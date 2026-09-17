#pragma once

#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>

namespace ppe_sample {
inline std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

// QoS 1 publisher: pending IDs are registered under the same lock used by the
// PUBACK callback, so a fast broker cannot ACK before insertion. One client
// publishes only events, no unrelated messages. Credentials never enter logs.
class mqtt_publisher {
    std::mutex mutex;
    std::condition_variable changed;
    std::set<int> pending;
    size_t sent = 0, acknowledged = 0;
    bool ready = false, refused = false;
    std::string topic;
    // Destroy the network client before callback state above.
    std::unique_ptr<cvedix_utils::cvedix_mqtt_client> client;
public:
    mqtt_publisher(const std::string& host, int port, const std::string& topic,
                   const std::string& client_id) : topic(topic) {
        if (host.empty() || port < 1 || port > 65535 || topic.empty() ||
            topic.find_first_of("+#") != std::string::npos)
            throw std::invalid_argument("Invalid MQTT host, port or publish topic");
        client = std::make_unique<cvedix_utils::cvedix_mqtt_client>(host, port, client_id);
        // libmosquitto's network loop handles reconnection; no second retry thread.
        client->set_auto_reconnect(false);
        const auto ca = env("HERAMIND_MQTT_CA_FILE");
        const auto cert = env("HERAMIND_MQTT_CERT_FILE"), key = env("HERAMIND_MQTT_KEY_FILE");
        if ((!ca.empty() || !cert.empty() || !key.empty()) && !client->set_tls(ca, cert, key))
            throw std::runtime_error("Cannot configure MQTT TLS certificate files");
        client->set_on_connect_callback([this](bool success) {
            std::lock_guard<std::mutex> lock(mutex);
            ready = success; refused = !success; changed.notify_all();
        });
        client->set_on_disconnect_callback([this] {
            std::lock_guard<std::mutex> lock(mutex);
            ready = false; changed.notify_all();
        });
        client->set_on_publish_callback([this](int mid) {
            std::lock_guard<std::mutex> lock(mutex);
            if (pending.erase(mid)) ++acknowledged;
            changed.notify_all();
        });
        if (!client->connect(env("HERAMIND_MQTT_USERNAME"), env("HERAMIND_MQTT_PASSWORD")))
            throw std::runtime_error("Cannot start MQTT connection");
        std::unique_lock<std::mutex> lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(5), [this] { return ready || refused; }) || !ready)
            throw std::runtime_error("MQTT broker did not accept the connection within 5 seconds");
    }
    bool publish(const std::string& payload) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!ready) return false;
        const int mid = client->publish(topic, payload, 1, false);
        if (mid < 0) return false;
        pending.insert(mid); ++sent;
        return true;
    }
    bool flush() {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(10), [this] { return pending.empty(); });
    }
    std::pair<size_t, size_t> counts() {
        std::lock_guard<std::mutex> lock(mutex);
        return {sent, acknowledged};
    }
};
} // namespace ppe_sample
