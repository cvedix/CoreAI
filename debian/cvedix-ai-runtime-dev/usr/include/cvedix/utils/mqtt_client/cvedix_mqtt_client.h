#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <ctime>

// Forward declarations
struct mosquitto;
struct mosquitto_message;

namespace cvedix_utils {
    /**
     * MQTT Client wrapper using libmosquitto
     * 
     * Features:
     * - Automatic connection and reconnection
     * - Thread-safe publishing
     * - Connection status monitoring
     * - Configurable keepalive and QoS
     */
    class cvedix_mqtt_client {
    public:
        // Callback function types
        using on_connect_callback = std::function<void(bool)>;
        using on_disconnect_callback = std::function<void()>;
        using on_publish_callback = std::function<void(int)>;
        using on_message_callback = std::function<void(const std::string& topic, const std::string& payload)>;
        
        /**
         * Constructor
         * @param broker_url MQTT broker URL (e.g., "anhoidong.datacenter.cvedix.com")
         * @param port MQTT broker port (default: 1883)
         * @param client_id Unique client ID (default: auto-generated)
         * @param keepalive Keepalive interval in seconds (default: 60)
         */
        cvedix_mqtt_client(
            const std::string& broker_url,
            int port = 1883,
            const std::string& client_id = "",
            int keepalive = 60
        );
        
        /**
         * Destructor - automatically disconnects
         */
        ~cvedix_mqtt_client();
        
        /**
         * Connect to MQTT broker
         * @param username Optional username for authentication
         * @param password Optional password for authentication
         * @return true if connection initiated successfully
         */
        bool connect(const std::string& username = "", const std::string& password = "");
        
        /**
         * Disconnect from MQTT broker
         */
        void disconnect();
        
        /**
         * Publish message to topic
         * @param topic MQTT topic
         * @param payload Message payload
         * @param qos Quality of Service (0, 1, or 2)
         * @param retain Retain flag
         * @return message ID if successful, -1 on error
         */
        int publish(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false);
        
        /**
         * Subscribe to topic
         * @param topic MQTT topic (supports wildcards: + for single level, # for multi-level)
         * @param qos Quality of Service (0, 1, or 2)
         * @return true if subscription successful
         */
        bool subscribe(const std::string& topic, int qos = 1);
        
        /**
         * Unsubscribe from topic
         * @param topic MQTT topic
         * @return true if unsubscription successful
         */
        bool unsubscribe(const std::string& topic);
        
        /**
         * Check if client is connected
         * @return true if connected
         */
        bool is_connected() const;
        
        /**
         * Get connection status
         * @return true if connected and ready to publish
         */
        bool is_ready() const;
        
        /**
         * Set callback for connection events
         */
        void set_on_connect_callback(on_connect_callback callback);
        
        /**
         * Set callback for disconnection events
         */
        void set_on_disconnect_callback(on_disconnect_callback callback);
        
        /**
         * Set callback for publish events
         */
        void set_on_publish_callback(on_publish_callback callback);
        
        /**
         * Set callback for received messages (subscribe)
         */
        void set_on_message_callback(on_message_callback callback);
        
        /**
         * Enable automatic reconnection
         * @param enable Enable/disable auto-reconnect
         * @param reconnect_interval_ms Reconnection interval in milliseconds
         */
        void set_auto_reconnect(bool enable, int reconnect_interval_ms = 5000);
        
        /**
         * Get last error message
         */
        std::string get_last_error() const;
        
    private:
        // Mosquitto instance
        mosquitto* mosq_;
        
        // Connection parameters
        std::string broker_url_;
        int port_;
        std::string client_id_;
        int keepalive_;
        std::string username_;
        std::string password_;
        
        // Connection state
        std::atomic<bool> connected_;
        std::atomic<bool> connecting_;
        std::mutex publish_mutex_;
        
        // Auto-reconnect
        std::atomic<bool> auto_reconnect_enabled_;
        int reconnect_interval_ms_;
        std::thread reconnect_thread_;
        std::atomic<bool> should_stop_reconnect_;
        
        // Callbacks
        on_connect_callback on_connect_cb_;
        on_disconnect_callback on_disconnect_cb_;
        on_publish_callback on_publish_cb_;
        on_message_callback on_message_cb_;
        
        // Error handling
        mutable std::string last_error_;
        
        // Internal methods
        void reconnect_loop();
        static void on_connect_wrapper(struct mosquitto* mosq, void* obj, int rc);
        static void on_disconnect_wrapper(struct mosquitto* mosq, void* obj, int rc);
        static void on_publish_wrapper(struct mosquitto* mosq, void* obj, int mid);
        static void on_message_wrapper(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message);
        
        // Prevent copying
        cvedix_mqtt_client(const cvedix_mqtt_client&) = delete;
        cvedix_mqtt_client& operator=(const cvedix_mqtt_client&) = delete;
    };
}

