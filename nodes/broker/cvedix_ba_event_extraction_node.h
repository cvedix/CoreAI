/**
 * @file cvedix_ba_event_extraction_node.h
 * @brief BA event extraction broker node
 *
 * Extracts ba_results from frame_meta and serializes each event
 * into the reference JSON format for publishing via user callback.
 *
 * This node implements the broker pattern:
 *   format_msg()  -> build JSON array of events, push to broker queue
 *   broke_msg()  -> broker thread calls event_publisher callback
 *
 * Unlike the MQTT broker which serializes detection targets,
 * this node ONLY processes BA events (crowding, intrusion, crossline, etc.).
 *
 * @section extraction_usage Usage Example
 * @code
 * // Attach to MQTT broker
 * auto mqtt_broker = std::make_shared<cvedix_mqtt_broker_node>("mqtt_broker", ...);
 * auto event_broker = std::make_shared<cvedix_ba_event_extraction_node>(
 *     "ba_events",
 *     "my-pipeline-instance-uuid",
 *     [&mqtt_broker](const std::string& json) {
 *         mqtt_broker->push_event(json);
 *     },
 *     false  // include_crop_images
 * );
 * event_broker->attach_to({ba_node_1, ba_node_2});
 * @endcode
 *
 * @see cvedix_mqtt_broker_node
 * @see cvedix_webhook_broker_node
 * @see cvedix_kafka_broker_node
 * @see cvedix_sse_broker_node
 */

#pragma once

#include "cvedix_msg_broker_node.h"
#include <functional>
#include <string>
#include <sstream>
#include <map>

namespace cvedix_nodes {

    /**
     * @brief BA event extraction and JSON serialization broker
     *
     * Processes ba_results and converts each event to the reference JSON
     * format with normalized coordinates, UUIDs, and ISO 8601 timestamps.
     *
     * Implements the standard broker pattern:
     * - format_msg(): serializes ba_results to JSON array, sets msg output
     * - broke_msg():  called by broker thread to invoke event_publisher
     *
     * For immediate sending (SSE, webhook), use push_event() directly.
     */
    class cvedix_ba_event_extraction_node : public cvedix_msg_broker_node {
    private:
        /// @brief User-provided event publisher callback
        std::function<void(const std::string&)> event_publisher = nullptr;

        /// @brief Analytics pipeline instance ID (UUID)
        std::string instance_id = "";

        /// @brief Whether to include base64 crop images in output
        bool include_crop_images = false;

        /// @brief Maps BA type to $id schema identifier
        static std::string ba_type_to_schema_id(cvedix_objects::cvedix_ba_type type);

        /// @brief Serialize a single BA result to JSON string
        std::string serialize_event(
            const std::shared_ptr<cvedix_objects::cvedix_ba_result>& ba) const;

    protected:
        /**
         * @brief Serialize ba_results to JSON array
         * @param meta Frame meta containing ba_results
         * @param[out] msg Output JSON array string
         */
        void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta,
                        std::string& msg) override;

        /**
         * @brief Publish JSON via event_publisher callback
         * @param msg JSON array from format_msg
         */
        void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param instance_id Analytics pipeline instance UUID
         * @param event_publisher Callback to publish each event JSON
         * @param include_crop_images Include base64 crop images in output
         */
        cvedix_ba_event_extraction_node(
            std::string node_name,
            std::string instance_id = "",
            std::function<void(const std::string&)> event_publisher = nullptr,
            bool include_crop_images = false);

        ~cvedix_ba_event_extraction_node();

        /**
         * @brief Set event publisher callback
         * @param publisher Function to publish event JSON
         */
        void set_event_publisher(std::function<void(const std::string&)> publisher);

        /**
         * @brief Set analytics pipeline instance ID
         * @param id UUID of the analytics pipeline
         */
        void set_instance_id(const std::string& id);

        /**
         * @brief Push event JSON directly (bypass broker queue)
         *
         * Immediately invokes event_publisher without going through
         * the broker queue. Useful for SSE or webhook where immediate
         * sending is preferred.
         *
         * @param event_json The serialized event JSON string
         */
        void push_event(const std::string& event_json);
    };

} // namespace cvedix_nodes
