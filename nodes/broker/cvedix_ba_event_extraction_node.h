/**
 * @file cvedix_ba_event_extraction_node.h
 * @brief BA event extraction broker node
 *
 * Extracts ba_results from frame_meta and serializes each event
 * into the reference JSON format for publishing via user callback.
 *
 * Unlike the MQTT broker which serializes detection targets,
 * this node ONLY processes BA events (crowding, intrusion, crossline, etc.).
 *
 * @section extraction_usage Usage Example
 * @code
 * auto event_broker = std::make_shared<cvedix_ba_event_extraction_node>(
 *     "ba_events",
 *     "my-pipeline-instance-uuid",
 *     [&mqtt_client](const std::string& event_json) {
 *         mqtt_client->publish("analytics/events", event_json);
 *     },
 *     true  // include_crop_images
 * );
 * event_broker->attach_to({ba_node_1, ba_node_2});
 * @endcode
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

    protected:
        void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta,
                        std::string& msg) override;
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

        void set_event_publisher(std::function<void(const std::string&)> publisher);
        void set_instance_id(const std::string& id);
    };

} // namespace cvedix_nodes
