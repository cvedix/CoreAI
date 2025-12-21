/**
 * @file cvedix_json_mqtt_broker_node.h
 * @brief Flexible MQTT broker with user-provided publisher callback
 * 
 * Transport-layer broker that serializes to JSON and publishes via user-provided
 * MQTT callback. No MQTT library dependency - user handles connection.
 * 
 * @section mqtt_features Features
 * - Custom JSON transformation function
 * - User-provided MQTT publisher (library-agnostic)
 * - Asynchronous (non-blocking)
 * 
 * @section mqtt_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_json_mqtt_broker_node>(
 *     "mqtt_broker",
 *     cvedix_broke_for::NORMAL,
 *     50, 200,
 *     // JSON transformer (optional)
 *     [](const std::string& json) { 
 *         return "{\"ts\":" + std::to_string(time(nullptr)) + ",\"data\":" + json + "}"; 
 *     },
 *     // MQTT publisher
 *     [&mqtt_client](const std::string& json) {
 *         mqtt_client->publish("detections/topic", json);
 *     }
 * );
 * @endcode
 * 
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#include <sstream>
#include <functional>
#include <string>

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {

    /**
     * @brief Flexible MQTT JSON broker
     * 
     * User provides publisher callback - no MQTT library dependency.
     * Supports custom JSON transformation.
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_json_mqtt_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Optional JSON transformation function (input → output)
        std::function<std::string(const std::string&)> json_transformer = nullptr;
        
        /// @brief User-provided MQTT publisher function
        std::function<void(const std::string&)> mqtt_publisher = nullptr;
        
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
         * @param node_name Unique node identifier
         * @param broke_for Target type
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         * @param json_transformer Optional JSON transformation function
         * @param mqtt_publisher User's MQTT publish function
         */
        cvedix_json_mqtt_broker_node(
            std::string node_name,
            cvedix_broke_for broke_for = cvedix_broke_for::NORMAL,
            int broking_cache_warn_threshold = 50,
            int broking_cache_ignore_threshold = 200,
            std::function<std::string(const std::string&)> json_transformer = nullptr,
            std::function<void(const std::string&)> mqtt_publisher = nullptr);
        
        /// @brief Destructor
        ~cvedix_json_mqtt_broker_node();
        
        /**
         * @brief Set JSON transformer
         * @param transformer Function: input JSON → transformed JSON
         */
        void set_json_transformer(std::function<std::string(const std::string&)> transformer);
        
        /**
         * @brief Get current JSON transformer
         * @return Current transformer function
         */
        std::function<std::string(const std::string&)> get_json_transformer() const;
        
        /**
         * @brief Set MQTT publisher
         * @param publisher Function to publish JSON
         */
        void set_mqtt_publisher(std::function<void(const std::string&)> publisher);
        
        /**
         * @brief Get current MQTT publisher
         * @return Current publisher function
         */
        std::function<void(const std::string&)> get_mqtt_publisher() const;
        
        /**
         * @brief Set max input queue size
         * @param size Queue size (increase for high FPS)
         */
        void set_max_queue_size(int size) { max_in_queue_size = size; }
    };
}
