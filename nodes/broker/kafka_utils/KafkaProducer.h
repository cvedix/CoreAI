/**
 * @file KafkaProducer.h
 * @brief Apache Kafka producer wrapper for Core AI Runtime
 * 
 * This file provides a simplified C++ wrapper around librdkafka for
 * publishing messages to Apache Kafka topics.
 * 
 * @section kafka_prereq Prerequisites
 * - librdkafka-dev: `apt-get install librdkafka-dev`
 * - Compile with `-DCVEDIX_WITH_KAFKA` flag
 * 
 * @section kafka_usage Usage Example
 * @code
 * // Create producer for a topic
 * KafkaProducer producer("localhost:9092", "detection-results", 0);
 * 
 * // Push JSON message
 * std::string json = R"({"event": "crossline", "count": 5})";
 * producer.pushMessage(json);
 * @endcode
 * 
 * @see cvedix_json_kafka_broker_node Uses this class for Kafka message publishing
 */

#pragma once

#ifdef CVEDIX_WITH_KAFKA
#include <string>
#include <iostream>

#include <librdkafka/rdkafkacpp.h>

/**
 * @brief Wrapper class for Apache Kafka producer
 * 
 * Provides a simplified interface for publishing messages to Kafka topics.
 * Handles connection management, configuration, and message delivery.
 * 
 * @note Requires librdkafka (v0.11.3+ recommended)
 * @note Only compiled when CVEDIX_WITH_KAFKA is defined
 */
class KafkaProducer
{
public:
    /**
     * @brief Constructor - connects to Kafka broker
     * 
     * @param brokers Comma-separated list of broker addresses (e.g., "localhost:9092")
     * @param topic Kafka topic name to publish to
     * @param partition Partition number (-1 for auto-partitioning)
     */
    explicit KafkaProducer(const std::string& brokers, const std::string& topic, int partition);

    /**
     * @brief Publish a message to the Kafka topic
     * 
     * Message is sent asynchronously. Delivery status is handled by internal callbacks.
     * 
     * @param str Message content (typically JSON string)
     */
    void pushMessage(const std::string& str);

    /// @brief Destructor - flushes pending messages and disconnects
    ~KafkaProducer();


private:
    /// @brief Kafka broker address(es)
    std::string m_brokers;
    /// @brief Target topic name
    std::string m_topicStr;
    /// @brief Target partition number
    int m_partition;

    /// @brief Global configuration object
    RdKafka::Conf* m_config;
    /// @brief Topic-specific configuration
    RdKafka::Conf* m_topicConfig;
    /// @brief Topic handle
    RdKafka::Topic* m_topic;
    /// @brief Producer instance
    RdKafka::Producer* m_producer;

    /// @brief Delivery report callback
    RdKafka::DeliveryReportCb* m_dr_cb;
    /// @brief Event callback
    RdKafka::EventCb* m_event_cb;
    /// @brief Partitioner callback
    RdKafka::PartitionerCb* m_partitioner_cb;
};
#endif