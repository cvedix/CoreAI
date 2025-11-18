

#include "cvedix_xml_socket_broker_node.h"


namespace cvedix_nodes {
        
    cvedix_xml_socket_broker_node::cvedix_xml_socket_broker_node(std::string node_name,
                                                        std::string des_ip,
                                                        int des_port,
                                                        cvedix_broke_for broke_for, 
                                                        int broking_cache_warn_threshold, 
                                                        int broking_cache_ignore_threshold):
                                                        cvedix_msg_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold),
                                                        des_ip(des_ip),
                                                        des_port(des_port) {
        udp_writer = kissnet::udp_socket(kissnet::endpoint(des_ip, des_port));
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [message broker] set des_ip as `%s` and des_port as [%d]", node_name.c_str(), des_ip.c_str(), des_port));
        this->initialized();
    }
    
    cvedix_xml_socket_broker_node::~cvedix_xml_socket_broker_node() {
        deinitialized();
        stop_broking();
    }

    void cvedix_xml_socket_broker_node::format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) {
        // serialize objects to xml by cereal
        std::stringstream msg_stream;
        {
            cereal::XMLOutputArchive xml_archive(msg_stream);
            
            // global values
            xml_archive(cereal::make_nvp("channel_index", meta->channel_index),
            cereal::make_nvp("frame_index", meta->frame_index),
            cereal::make_nvp("width", meta->frame.cols),
            cereal::make_nvp("height", meta->frame.rows),
            cereal::make_nvp("fps", meta->fps),
            cereal::make_nvp("broke_for", broke_fors.at(broke_for)));

            // serialize values according to broke_for
            if (broke_for == cvedix_broke_for::NORMAL) {
                xml_archive(cereal::make_nvp("target_size", meta->targets.size()), 
                            cereal::make_nvp("targets", meta->targets));
            }
            else if (broke_for ==  cvedix_broke_for::FACE) {
                xml_archive(cereal::make_nvp("face_target_size", meta->face_targets.size()),
                            cereal::make_nvp("face_targets", meta->face_targets));
            }
            else if (broke_for == cvedix_broke_for::TEXT) {
                xml_archive(cereal::make_nvp("text_target_size", meta->text_targets.size()),
                            cereal::make_nvp("text_targets", meta->text_targets));
            }
            else {
                throw "invalid broke_for!";
            }
        } // flush

        msg = msg_stream.str();   
    }

    void cvedix_xml_socket_broker_node::broke_msg(const std::string& msg) {
        // broke msg to socket by udp
        auto bytes_2_send = reinterpret_cast<const std::byte*>(msg.c_str());
        auto bytes_2_send_len = msg.size();
        udp_writer.send(bytes_2_send, bytes_2_send_len);
    }
}