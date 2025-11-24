#include "cvedix_mqtt_json_receiver.h"
#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include "third_party/nlohmann/json.hpp"
#include <iostream>
#include <algorithm>

namespace cvedix_utils {
    
    cvedix_mqtt_json_receiver::cvedix_mqtt_json_receiver(
        const std::string& broker_url,
        int port,
        const std::string& client_id)
        : mqtt_client_(std::make_unique<cvedix_mqtt_client>(
            broker_url, 
            port, 
            client_id.empty() ? "cvedix_json_receiver_" + std::to_string(std::time(nullptr)) : client_id))
    {
        // Set message callback
        mqtt_client_->set_on_message_callback([this](const std::string& topic, const std::string& payload) {
            handle_message(topic, payload);
        });
        
        // Set connect callback để re-subscribe sau khi reconnect
        mqtt_client_->set_on_connect_callback([this](bool success) {
            if (success) {
                resubscribe_all();
            }
        });
    }
    
    cvedix_mqtt_json_receiver::~cvedix_mqtt_json_receiver() {
        disconnect();
    }
    
    bool cvedix_mqtt_json_receiver::connect(const std::string& username, const std::string& password) {
        return mqtt_client_->connect(username, password);
    }
    
    void cvedix_mqtt_json_receiver::disconnect() {
        mqtt_client_->disconnect();
    }
    
    bool cvedix_mqtt_json_receiver::subscribe(const std::string& topic, int qos) {
        if (!mqtt_client_->is_connected()) {
            return false;
        }
        
        bool success = mqtt_client_->subscribe(topic, qos);
        if (success) {
            std::lock_guard<std::mutex> lock(topics_mutex_);
            // Chỉ thêm nếu chưa có
            if (std::find(subscribed_topics_.begin(), subscribed_topics_.end(), topic) == subscribed_topics_.end()) {
                subscribed_topics_.push_back(topic);
            }
        }
        return success;
    }
    
    bool cvedix_mqtt_json_receiver::subscribe_multiple(const std::vector<std::string>& topics, int qos) {
        bool all_success = true;
        for (const auto& topic : topics) {
            if (!subscribe(topic, qos)) {
                all_success = false;
            }
        }
        return all_success;
    }
    
    bool cvedix_mqtt_json_receiver::unsubscribe(const std::string& topic) {
        if (!mqtt_client_->is_connected()) {
            return false;
        }
        
        bool success = mqtt_client_->unsubscribe(topic);
        if (success) {
            std::lock_guard<std::mutex> lock(topics_mutex_);
            subscribed_topics_.erase(
                std::remove(subscribed_topics_.begin(), subscribed_topics_.end(), topic),
                subscribed_topics_.end()
            );
        }
        return success;
    }
    
    void cvedix_mqtt_json_receiver::set_json_callback(json_callback callback) {
        json_cb_ = callback;
    }
    
    void cvedix_mqtt_json_receiver::set_raw_callback(raw_callback callback) {
        raw_cb_ = callback;
    }
    
    bool cvedix_mqtt_json_receiver::is_connected() const {
        return mqtt_client_->is_connected();
    }
    
    void cvedix_mqtt_json_receiver::set_auto_reconnect(bool enable, int reconnect_interval_ms) {
        mqtt_client_->set_auto_reconnect(enable, reconnect_interval_ms);
    }
    
    std::string cvedix_mqtt_json_receiver::get_last_error() const {
        return mqtt_client_->get_last_error();
    }
    
    bool cvedix_mqtt_json_receiver::is_valid_json(const std::string& json_str) {
        try {
            nlohmann::json::parse(json_str);
            return true;
        } catch (const nlohmann::json::parse_error&) {
            return false;
        } catch (...) {
            return false;
        }
    }
    
    void cvedix_mqtt_json_receiver::handle_message(const std::string& topic, const std::string& payload) {
        // Gọi raw callback nếu có (cho mọi message)
        if (raw_cb_) {
            raw_cb_(topic, payload);
        }
        
        // Kiểm tra và parse JSON
        if (is_valid_json(payload)) {
            // Gọi JSON callback nếu có
            if (json_cb_) {
                json_cb_(topic, payload);
            }
        } else {
            // Log warning nếu không phải JSON hợp lệ
            std::cerr << "[MQTT JSON Receiver] Received non-JSON message on topic: " << topic 
                      << ", length: " << payload.length() << std::endl;
        }
    }
    
    void cvedix_mqtt_json_receiver::resubscribe_all() {
        std::lock_guard<std::mutex> lock(topics_mutex_);
        for (const auto& topic : subscribed_topics_) {
            mqtt_client_->subscribe(topic, 1); // Default QoS = 1
        }
    }
}

