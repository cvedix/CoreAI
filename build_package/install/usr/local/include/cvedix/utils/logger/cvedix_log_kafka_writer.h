#pragma once

#ifdef CVEDIX_WITH_KAFKA
#include <memory>
// to-do: refactor for file structure
#include "cvedix/nodes/broker/kafka_utils/KafkaProducer.h"

namespace cvedix_utils {
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

        // for << operator
        cvedix_log_kafka_writer& operator<<(std::string log);
    };
}
#endif