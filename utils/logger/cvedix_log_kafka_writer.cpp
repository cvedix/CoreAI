#ifdef CVEDIX_WITH_KAFKA
#include "cvedix_log_kafka_writer.h"

namespace cvedix_utils {
    
    cvedix_log_kafka_writer::cvedix_log_kafka_writer(/* args */) {

    }
    
    cvedix_log_kafka_writer::~cvedix_log_kafka_writer() {

    }

    void cvedix_log_kafka_writer::write(std::string log) {
        if (!inited) {
            throw "cvedix_log_kafka_writer not initialized!";
        }
        kafka_producer->pushMessage(log);
    }

    void cvedix_log_kafka_writer::init(std::string kafka_servers, std::string topic_name) {
        kafka_producer = std::make_shared<KafkaProducer>(kafka_servers, topic_name, 0);
        inited = true;
    }

    void cvedix_log_kafka_writer::shutdown() {
        if (inited) {
            kafka_producer.reset();
            inited = false;
        }
    }

    // for << operator
    cvedix_log_kafka_writer& cvedix_log_kafka_writer::operator<<(std::string log) {
        write(log);
        return *this;
    }
}
#endif