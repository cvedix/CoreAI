#pragma once

#include <sstream>
#include <functional>
#include <string>

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {
    // JSON to MQTT broker node (transport layer)
    // This node receives frame_meta, serializes to JSON (with customizable structure),
    // and publishes to MQTT broker via user-provided MQTT publisher function
    // 
    // Features:
    // - Custom JSON transformation function (allows user to modify JSON structure)
    // - User-provided MQTT publisher function (flexible, no dependency on specific MQTT library)
    // - Works asynchronously (does not block pipeline)
    // - Acts as a transport layer only - user handles MQTT connection/publishing
    class cvedix_mqtt_broker_node: public cvedix_msg_broker_node
    {
    private:
        // Custom JSON transformation function
        // Input: original JSON string from format_msg
        // Output: transformed JSON string to be sent via MQTT
        // If nullptr, original JSON will be sent without transformation
        std::function<std::string(const std::string&)> json_transformer = nullptr;
        
        // Custom MQTT publisher function
        // Input: JSON string to publish
        // User is responsible for MQTT connection and publishing
        // If nullptr, message will be logged but not sent
        std::function<void(const std::string&)> mqtt_publisher = nullptr;
        
    protected:
        // Serialize frame_meta to JSON (can be customized)
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        
        // Publish JSON message via user-provided MQTT publisher function
        virtual void broke_msg(const std::string& msg) override;
        
    public:
        // Constructor
        // json_transformer: Optional function to transform JSON before sending
        //   If nullptr, original JSON will be sent
        //   Example: [](const std::string& json) { 
        //       return "{\"timestamp\": " + std::to_string(time(nullptr)) + ", \"data\": " + json + "}"; 
        //   }
        // mqtt_publisher: Function to publish JSON to MQTT broker
        //   User must handle MQTT connection and publishing
        //   Example: [](const std::string& json) {
        //       // Your MQTT publishing code here
        //       mqtt_client->publish("topic", json);
        //   }
        cvedix_mqtt_broker_node(
            std::string node_name,
            cvedix_broke_for broke_for = cvedix_broke_for::NORMAL,
            int broking_cache_warn_threshold = 50,
            int broking_cache_ignore_threshold = 200,
            std::function<std::string(const std::string&)> json_transformer = nullptr,
            std::function<void(const std::string&)> mqtt_publisher = nullptr);
        
        ~cvedix_mqtt_broker_node();
        
        // Set custom JSON transformer function
        void set_json_transformer(std::function<std::string(const std::string&)> transformer);
        
        // Get current JSON transformer function
        std::function<std::string(const std::string&)> get_json_transformer() const;
        
        // Set MQTT publisher function
        void set_mqtt_publisher(std::function<void(const std::string&)> publisher);
        
        // Get current MQTT publisher function
        std::function<void(const std::string&)> get_mqtt_publisher() const;
    };
}

