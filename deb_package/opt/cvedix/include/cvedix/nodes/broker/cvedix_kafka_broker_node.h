#pragma once

#ifdef CVEDIX_WITH_KAFKA
#include <sstream>

#include "cvedix_msg_broker_node.h"
#include "kafka_utils/KafkaProducer.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {
    // message broker node, broke json data to kafka.
    class cvedix_kafka_broker_node: public cvedix_msg_broker_node
    {
    private:
        /* data */
        std::shared_ptr<KafkaProducer> kafka_producer = nullptr;
    protected:
        // to json
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        // to console
        virtual void broke_msg(const std::string& msg) override;
    public:
        cvedix_kafka_broker_node(std::string node_name, 
                                    std::string kafka_servers = "127.0.0.1:9092",
                                    std::string topic_name = "videopipe_topic",
                                    cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                    int broking_cache_warn_threshold = 50, 
                                    int broking_cache_ignore_threshold = 200);
        ~cvedix_kafka_broker_node();
    };
}
#endif