/**
 * @file cvedix_json_kafka_broker_node.h
 * @brief Apache Kafka broker for JSON detection results
 * 
 * Publishes detection results as JSON to Apache Kafka topics.
 * 
 * @section kafka_prereq Prerequisites
 * - Compile with `-DCVEDIX_WITH_KAFKA`
 * - librdkafka-dev: `apt-get install librdkafka-dev`
 * 
 * @section kafka_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_json_kafka_broker_node>(
 *     "kafka_broker",
 *     "kafka-server:9092",
 *     "detection-topic"
 * );
 * broker->attach_to({detector_node});
 * @endcode
 * 
 * @see KafkaProducer Underlying Kafka producer wrapper
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#ifdef CVEDIX_WITH_KAFKA
#include <sstream>

#include "cvedix_msg_broker_node.h"
#include "kafka_utils/KafkaProducer.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {

    /**
     * @brief Kafka JSON broker
     * 
     * Publishes frame_meta as JSON to Kafka topics.
     * 
     * @note Only available when CVEDIX_WITH_KAFKA is defined
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_json_kafka_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Kafka producer instance
        std::shared_ptr<KafkaProducer> kafka_producer = nullptr;

    protected:
        /**
         * @brief Serialize to JSON
         * @param meta Frame meta to serialize
         * @param[out] msg Output JSON string
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief Publish to Kafka
         * @param msg JSON message to publish
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param kafka_servers Kafka broker addresses (default: "127.0.0.1:9092")
         * @param topic_name Kafka topic name (default: "sdk_topic")
         * @param broke_for Target type
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         */
        cvedix_json_kafka_broker_node(std::string node_name, 
                                    std::string kafka_servers = "127.0.0.1:9092",
                                    std::string topic_name = "sdk_topic",
                                    cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                    int broking_cache_warn_threshold = 50, 
                                    int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_json_kafka_broker_node();
    };
}
#endif