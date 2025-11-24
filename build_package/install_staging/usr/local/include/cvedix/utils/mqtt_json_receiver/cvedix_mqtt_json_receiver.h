#pragma once

#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace cvedix_utils {
    /**
     * MQTT JSON Receiver - Utility class để nhận và xử lý JSON từ MQTT
     * 
     * Features:
     * - Subscribe đến MQTT topics
     * - Nhận JSON messages và parse
     * - Callback để xử lý JSON đã parse
     * - Auto-reconnect
     * - Thread-safe
     */
    class cvedix_mqtt_json_receiver {
    public:
        /**
         * Callback function type cho JSON messages
         * @param topic MQTT topic mà message được nhận
         * @param json_data JSON string đã được parse và validate
         */
        using json_callback = std::function<void(const std::string& topic, const std::string& json_data)>;
        
        /**
         * Callback function type cho raw messages (trước khi parse JSON)
         * @param topic MQTT topic
         * @param raw_data Raw message payload
         */
        using raw_callback = std::function<void(const std::string& topic, const std::string& raw_data)>;
        
        /**
         * Constructor
         * @param broker_url MQTT broker URL
         * @param port MQTT broker port (default: 1883)
         * @param client_id Unique client ID (optional)
         */
        cvedix_mqtt_json_receiver(
            const std::string& broker_url,
            int port = 1883,
            const std::string& client_id = ""
        );
        
        /**
         * Destructor
         */
        ~cvedix_mqtt_json_receiver();
        
        /**
         * Kết nối đến MQTT broker
         * @param username Optional username
         * @param password Optional password
         * @return true nếu kết nối thành công
         */
        bool connect(const std::string& username = "", const std::string& password = "");
        
        /**
         * Ngắt kết nối
         */
        void disconnect();
        
        /**
         * Subscribe đến một topic
         * @param topic MQTT topic (hỗ trợ wildcards: +, #)
         * @param qos Quality of Service (0, 1, hoặc 2)
         * @return true nếu subscribe thành công
         */
        bool subscribe(const std::string& topic, int qos = 1);
        
        /**
         * Subscribe đến nhiều topics
         * @param topics Vector các topics
         * @param qos Quality of Service
         * @return true nếu tất cả subscribe thành công
         */
        bool subscribe_multiple(const std::vector<std::string>& topics, int qos = 1);
        
        /**
         * Unsubscribe từ một topic
         * @param topic MQTT topic
         * @return true nếu unsubscribe thành công
         */
        bool unsubscribe(const std::string& topic);
        
        /**
         * Set callback để xử lý JSON messages
         * @param callback Function được gọi khi nhận JSON message hợp lệ
         */
        void set_json_callback(json_callback callback);
        
        /**
         * Set callback để xử lý raw messages (trước khi parse JSON)
         * @param callback Function được gọi với mọi message, kể cả không phải JSON
         */
        void set_raw_callback(raw_callback callback);
        
        /**
         * Kiểm tra kết nối
         * @return true nếu đã kết nối
         */
        bool is_connected() const;
        
        /**
         * Bật/tắt auto-reconnect
         * @param enable Enable/disable
         * @param reconnect_interval_ms Interval giữa các lần reconnect (ms)
         */
        void set_auto_reconnect(bool enable, int reconnect_interval_ms = 5000);
        
        /**
         * Lấy lỗi cuối cùng
         */
        std::string get_last_error() const;
        
        /**
         * Validate JSON string
         * @param json_str JSON string cần validate
         * @return true nếu là JSON hợp lệ
         */
        static bool is_valid_json(const std::string& json_str);
        
    private:
        std::unique_ptr<cvedix_mqtt_client> mqtt_client_;
        json_callback json_cb_;
        raw_callback raw_cb_;
        std::vector<std::string> subscribed_topics_;
        std::mutex topics_mutex_;
        
        /**
         * Xử lý message từ MQTT
         */
        void handle_message(const std::string& topic, const std::string& payload);
        
        /**
         * Re-subscribe tất cả topics sau khi reconnect
         */
        void resubscribe_all();
    };
}

