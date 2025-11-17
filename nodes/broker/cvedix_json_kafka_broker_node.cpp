#ifdef CVEDIX_WITH_KAFKA
#include "cvedix_json_kafka_broker_node.h"

namespace cvedix_nodes {
        
    cvedix_json_kafka_broker_node::cvedix_json_kafka_broker_node(std::string node_name, 
                                                            std::string kafka_servers,
                                                            std::string topic_name,
                                                            cvedix_broke_for broke_for, 
                                                            int broking_cache_warn_threshold, 
                                                            int broking_cache_ignore_threshold):
                                                            cvedix_msg_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold) {
        CVEDIX_INFO(cvedix_utils::string_format("[%s] kafka_servers:[%s] topic_name:[%s]", node_name.c_str(), kafka_servers.c_str(), topic_name.c_str()));
        kafka_producer = std::make_shared<KafkaProducer>(kafka_servers, topic_name, 0);
        this->initialized();
    }
    
    cvedix_json_kafka_broker_node::~cvedix_json_kafka_broker_node() {
        deinitialized();
        stop_broking();
    }
    
    void cvedix_json_kafka_broker_node::format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) {
        // serialize objects to json by cereal
        std::stringstream msg_stream;
        {
            cereal::JSONOutputArchive json_archive(msg_stream);
            
            // global values
            json_archive(cereal::make_nvp("channel_index", meta->channel_index),
            cereal::make_nvp("frame_index", meta->frame_index),
            cereal::make_nvp("width", meta->frame.cols),
            cereal::make_nvp("height", meta->frame.rows),
            cereal::make_nvp("fps", meta->fps),
            cereal::make_nvp("broke_for", broke_fors.at(broke_for)));

            // serialize values according to broke_for
            if (broke_for == cvedix_broke_for::NORMAL) {
                json_archive(cereal::make_nvp("target_size", meta->targets.size()), 
                            cereal::make_nvp("targets", meta->targets));
            }
            else if (broke_for ==  cvedix_broke_for::FACE) {
                json_archive(cereal::make_nvp("face_target_size", meta->face_targets.size()),
                            cereal::make_nvp("face_targets", meta->face_targets));
            }
            else if (broke_for == cvedix_broke_for::TEXT) {
                json_archive(cereal::make_nvp("text_target_size", meta->text_targets.size()),
                            cereal::make_nvp("text_targets", meta->text_targets));
            }
            else {
                throw "invalid broke_for!";
            }
        } // flush

        msg = msg_stream.str();
    }

    void cvedix_json_kafka_broker_node::broke_msg(const std::string& msg) {
        // broke msg to kafka by kafka client api
        if (kafka_producer != nullptr) {
            kafka_producer->pushMessage(msg);
        }
    }
}
#endif