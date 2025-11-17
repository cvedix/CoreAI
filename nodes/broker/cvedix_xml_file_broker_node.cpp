#include "cvedix_xml_file_broker_node.h"

namespace cvedix_nodes {
    
    cvedix_xml_file_broker_node::cvedix_xml_file_broker_node(std::string node_name, 
                                                    cvedix_broke_for broke_for, 
                                                    std::string file_path_and_name,
                                                    int broking_cache_warn_threshold, 
                                                    int broking_cache_ignore_threshold):
                                                    cvedix_msg_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold) {
        // open file
        if (file_path_and_name.empty()) {
            file_path_and_name = "cvedix_broker_" + cvedix_utils::time_format(NOW, "<hour>-<min>-<sec>-<mili>") + ".xml";
        }
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [message broker] set broking file path as `%s`", node_name.c_str(), file_path_and_name.c_str()));
        xml_writer.open(file_path_and_name);

        this->initialized();
    }
    
    cvedix_xml_file_broker_node::~cvedix_xml_file_broker_node() {
        deinitialized();
        stop_broking();
    }
    
    void cvedix_xml_file_broker_node::format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) {
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

    void cvedix_xml_file_broker_node::broke_msg(const std::string& msg) {
        // broke msg to file by ofstream
        if (xml_writer.is_open()) {
            xml_writer << msg << std::endl;
        }
        else {
            // TO-DO
        }
    }
}