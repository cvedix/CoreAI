#include "cvedix_mqtt_client.h"
#include <mosquitto.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <atomic>
#include <mutex>

namespace cvedix_utils {
    
    // Global reference count for mosquitto library init/cleanup
    static std::atomic<int> g_mosquitto_ref_count{0};
    static std::mutex g_mosquitto_init_mutex;

    cvedix_mqtt_client::cvedix_mqtt_client(
        const std::string& broker_url,
        int port,
        const std::string& client_id,
        int keepalive)
        : broker_url_(broker_url)
        , port_(port)
        , client_id_(client_id.empty() ? "cvedix_client_" + std::to_string(std::time(nullptr)) : client_id)
        , keepalive_(keepalive)
        , connected_(false)
        , connecting_(false)
        , auto_reconnect_enabled_(true)
        , reconnect_interval_ms_(5000)
        , should_stop_reconnect_(false)
        , mosq_(nullptr)
    {
        // Initialize mosquitto library (reference-counted, thread-safe)
        {
            std::lock_guard<std::mutex> lock(g_mosquitto_init_mutex);
            if (g_mosquitto_ref_count.fetch_add(1) == 0) {
                mosquitto_lib_init();
            }
        }
        
        // Create mosquitto instance
        mosq_ = mosquitto_new(client_id_.c_str(), true, this);
        if (!mosq_) {
            last_error_ = "Failed to create mosquitto instance";
            return;
        }
        
        // Set callbacks
        mosquitto_connect_callback_set(mosq_, on_connect_wrapper);
        mosquitto_disconnect_callback_set(mosq_, on_disconnect_wrapper);
        mosquitto_publish_callback_set(mosq_, on_publish_wrapper);
        mosquitto_message_callback_set(mosq_, on_message_wrapper);
        
        // Set will message (optional - notify when client disconnects unexpectedly)
        // mosquitto_will_set(mosq_, "cvedix/status", 6, "offline", 1, true);
    }
    
    cvedix_mqtt_client::~cvedix_mqtt_client() {
        // Stop reconnect thread
        should_stop_reconnect_ = true;
        if (reconnect_thread_.joinable()) {
            reconnect_thread_.join();
        }
        
        // Disconnect if connected
        if (connected_) {
            disconnect();
        }
        
        // Cleanup mosquitto instance
        if (mosq_) {
            mosquitto_destroy(mosq_);
            mosq_ = nullptr;
        }
        
        // Cleanup mosquitto library (reference-counted, only when last instance)
        {
            std::lock_guard<std::mutex> lock(g_mosquitto_init_mutex);
            if (g_mosquitto_ref_count.fetch_sub(1) == 1) {
                mosquitto_lib_cleanup();
            }
        }
    }
    
    bool cvedix_mqtt_client::connect(const std::string& username, const std::string& password) {
        if (!mosq_) {
            last_error_ = "Mosquitto instance not initialized";
            return false;
        }
        
        if (connecting_ || connected_) {
            return connected_;
        }
        
        username_ = username;
        password_ = password;
        
        // Set credentials if provided
        if (!username.empty() && !password.empty()) {
            int rc = mosquitto_username_pw_set(mosq_, username.c_str(), password.c_str());
            if (rc != MOSQ_ERR_SUCCESS) {
                last_error_ = "Failed to set username/password: " + std::string(mosquitto_strerror(rc));
                return false;
            }
        }
        
        // Start connection
        connecting_ = true;
        int rc = mosquitto_connect_async(mosq_, broker_url_.c_str(), port_, keepalive_);
        
        if (rc != MOSQ_ERR_SUCCESS) {
            connecting_ = false;
            last_error_ = "Failed to initiate connection: " + std::string(mosquitto_strerror(rc));
            return false;
        }
        
        // Start network loop in a separate thread
        mosquitto_loop_start(mosq_);
        
        // Wait a bit for connection to establish
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Start reconnect thread if auto-reconnect is enabled
        if (auto_reconnect_enabled_ && !reconnect_thread_.joinable()) {
            reconnect_thread_ = std::thread(&cvedix_mqtt_client::reconnect_loop, this);
        }
        
        return true;
    }
    
    void cvedix_mqtt_client::disconnect() {
        if (!mosq_ || !connected_) {
            return;
        }
        
        should_stop_reconnect_ = true;
        connected_ = false;
        connecting_ = false;
        
        mosquitto_disconnect(mosq_);
        mosquitto_loop_stop(mosq_, false);
    }
    
    int cvedix_mqtt_client::publish(const std::string& topic, const std::string& payload, int qos, bool retain) {
        if (!mosq_) {
            last_error_ = "Mosquitto instance not initialized";
            return -1;
        }
        
        if (!connected_) {
            last_error_ = "Not connected to broker";
            return -1;
        }
        
        // Clamp QoS to valid range
        if (qos < 0) qos = 0;
        if (qos > 2) qos = 2;
        
        std::lock_guard<std::mutex> lock(publish_mutex_);
        
        int mid = 0;
        int rc = mosquitto_publish(
            mosq_,
            &mid,
            topic.c_str(),
            payload.length(),
            payload.c_str(),
            qos,
            retain
        );
        
        if (rc != MOSQ_ERR_SUCCESS) {
            last_error_ = "Publish failed: " + std::string(mosquitto_strerror(rc));
            return -1;
        }
        
        return mid;
    }
    
    bool cvedix_mqtt_client::is_connected() const {
        return connected_;
    }
    
    bool cvedix_mqtt_client::is_ready() const {
        return connected_ && mosq_ != nullptr;
    }
    
    void cvedix_mqtt_client::set_on_connect_callback(on_connect_callback callback) {
        on_connect_cb_ = callback;
    }
    
    void cvedix_mqtt_client::set_on_disconnect_callback(on_disconnect_callback callback) {
        on_disconnect_cb_ = callback;
    }
    
    void cvedix_mqtt_client::set_on_publish_callback(on_publish_callback callback) {
        on_publish_cb_ = callback;
    }
    
    void cvedix_mqtt_client::set_on_message_callback(on_message_callback callback) {
        on_message_cb_ = callback;
    }
    
    bool cvedix_mqtt_client::subscribe(const std::string& topic, int qos) {
        if (!mosq_) {
            last_error_ = "Mosquitto instance not initialized";
            return false;
        }
        
        if (!connected_) {
            last_error_ = "Not connected to broker";
            return false;
        }
        
        // Clamp QoS to valid range
        if (qos < 0) qos = 0;
        if (qos > 2) qos = 2;
        
        int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), qos);
        
        if (rc != MOSQ_ERR_SUCCESS) {
            last_error_ = "Subscribe failed: " + std::string(mosquitto_strerror(rc));
            return false;
        }
        
        return true;
    }
    
    bool cvedix_mqtt_client::unsubscribe(const std::string& topic) {
        if (!mosq_) {
            last_error_ = "Mosquitto instance not initialized";
            return false;
        }
        
        if (!connected_) {
            last_error_ = "Not connected to broker";
            return false;
        }
        
        int rc = mosquitto_unsubscribe(mosq_, nullptr, topic.c_str());
        
        if (rc != MOSQ_ERR_SUCCESS) {
            last_error_ = "Unsubscribe failed: " + std::string(mosquitto_strerror(rc));
            return false;
        }
        
        return true;
    }
    
    void cvedix_mqtt_client::set_auto_reconnect(bool enable, int reconnect_interval_ms) {
        auto_reconnect_enabled_ = enable;
        reconnect_interval_ms_ = reconnect_interval_ms;
    }
    
    std::string cvedix_mqtt_client::get_last_error() const {
        return last_error_;
    }
    
    void cvedix_mqtt_client::reconnect_loop() {
        while (!should_stop_reconnect_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_interval_ms_));
            
            if (should_stop_reconnect_) {
                break;
            }
            
            if (!connected_ && !connecting_) {
                // Try to reconnect
                connecting_ = true;
                int rc = mosquitto_reconnect_async(mosq_);
                if (rc == MOSQ_ERR_SUCCESS) {
                    // Connection initiated, wait a bit
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                } else {
                    connecting_ = false;
                }
            }
        }
    }
    
    void cvedix_mqtt_client::on_connect_wrapper(struct mosquitto* mosq, void* obj, int rc) {
        cvedix_mqtt_client* client = static_cast<cvedix_mqtt_client*>(obj);
        if (!client) return;
        
        if (rc == 0) {
            // Connection successful
            client->connected_ = true;
            client->connecting_ = false;
            client->last_error_.clear();
            
            if (client->on_connect_cb_) {
                client->on_connect_cb_(true);
            }
        } else {
            // Connection failed
            client->connected_ = false;
            client->connecting_ = false;
            client->last_error_ = "Connection failed: " + std::string(mosquitto_connack_string(rc));
            
            if (client->on_connect_cb_) {
                client->on_connect_cb_(false);
            }
        }
    }
    
    void cvedix_mqtt_client::on_disconnect_wrapper(struct mosquitto* mosq, void* obj, int rc) {
        cvedix_mqtt_client* client = static_cast<cvedix_mqtt_client*>(obj);
        if (!client) return;
        
        client->connected_ = false;
        client->connecting_ = false;
        
        if (rc != 0) {
            client->last_error_ = "Unexpected disconnect: " + std::string(mosquitto_strerror(rc));
        }
        
        if (client->on_disconnect_cb_) {
            client->on_disconnect_cb_();
        }
    }
    
    void cvedix_mqtt_client::on_publish_wrapper(struct mosquitto* mosq, void* obj, int mid) {
        cvedix_mqtt_client* client = static_cast<cvedix_mqtt_client*>(obj);
        if (!client) return;
        
        if (client->on_publish_cb_) {
            client->on_publish_cb_(mid);
        }
    }
    
    void cvedix_mqtt_client::on_message_wrapper(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message) {
        cvedix_mqtt_client* client = static_cast<cvedix_mqtt_client*>(obj);
        if (!client || !message) return;
        
        if (client->on_message_cb_) {
            std::string topic(message->topic ? message->topic : "");
            std::string payload;
            
            if (message->payload && message->payloadlen > 0) {
                payload.assign(static_cast<const char*>(message->payload), message->payloadlen);
            }
            
            client->on_message_cb_(topic, payload);
        }
    }
}

