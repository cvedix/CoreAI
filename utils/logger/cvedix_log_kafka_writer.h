/**
 * @file cvedix_log_kafka_writer.h
 * @brief Log writer for Apache Kafka
 */

#pragma once

#ifdef CVEDIX_WITH_KAFKA
#include <memory>
#include "cvedix/nodes/broker/kafka_utils/KafkaProducer.h"

namespace cvedix_utils {
    /**
     * @brief Kafka log writer
     */
    class cvedix_log_kafka_writer
    {

    private:
        // ready to go
        bool inited = false;
        // wrapper producer
        std::shared_ptr<KafkaProducer> kafka_producer = nullptr;

    public:
        cvedix_log_kafka_writer(/* args */);
        ~cvedix_log_kafka_writer();

        // write log
        void write(std::string log);

        // initialize writer
        void init(std::string kafka_servers, std::string topic_name);

        // shutdown/cleanup writer resources
        void shutdown();

        // check if initialized
        bool is_inited() const { return inited; }

        // for << operator
        cvedix_log_kafka_writer& operator<<(std::string log);
    };
}
#endif