#include "cvedix_mqtt_broker_node.h"
#include <iostream>
#include <sstream>

namespace cvedix_nodes {
    
    cvedix_mqtt_broker_node::cvedix_mqtt_broker_node(
        std::string node_name,
        cvedix_broke_for broke_for,
        int broking_cache_warn_threshold,
        int broking_cache_ignore_threshold,
        std::function<std::string(const std::string&)> json_transformer,
        std::function<void(const std::string&)> mqtt_publisher):
        cvedix_msg_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold),
        json_transformer(json_transformer),
        mqtt_publisher(mqtt_publisher) {
        
        // Start the node's handle/dispatch threads (same as all other broker implementations)
        this->initialized();
        
        if (mqtt_publisher == nullptr) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] MQTT publisher function not set. Messages will be logged but not sent.", 
                node_name.c_str()));
        }
    }
    
    cvedix_mqtt_broker_node::~cvedix_mqtt_broker_node() {
        // Stop processing threads and broking thread (same pattern as all other broker implementations)
        deinitialized();
        stop_broking();
    }
    
    void cvedix_mqtt_broker_node::set_json_transformer(std::function<std::string(const std::string&)> transformer) {
        json_transformer = transformer;
    }
    
    std::function<std::string(const std::string&)> cvedix_mqtt_broker_node::get_json_transformer() const {
        return json_transformer;
    }
    
    void cvedix_mqtt_broker_node::set_mqtt_publisher(std::function<void(const std::string&)> publisher) {
        mqtt_publisher = publisher;
    }
    
    std::function<void(const std::string&)> cvedix_mqtt_broker_node::get_mqtt_publisher() const {
        return mqtt_publisher;
    }
    
    void cvedix_mqtt_broker_node::format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) {
        // Serialize objects to JSON by cereal (same as json_console_broker_node)
        try {
            std::stringstream msg_stream;
            {
                cereal::JSONOutputArchive json_archive(msg_stream);
                
                // Global values
                json_archive(cereal::make_nvp("channel_index", meta->channel_index),
                            cereal::make_nvp("frame_index", meta->frame_index),
                            cereal::make_nvp("width", meta->frame.cols),
                            cereal::make_nvp("height", meta->frame.rows),
                            cereal::make_nvp("fps", meta->fps),
                            cereal::make_nvp("broke_for", broke_fors.at(broke_for)));

                // Serialize values according to broke_for
                if (broke_for == cvedix_broke_for::NORMAL) {
                    json_archive(cereal::make_nvp("target_size", meta->targets.size()), 
                                cereal::make_nvp("targets", meta->targets));
                }
                else if (broke_for == cvedix_broke_for::FACE) {
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
            
            // Apply custom JSON transformation if provided
            if (json_transformer != nullptr) {
                try {
                    msg = json_transformer(msg);
                } catch (const std::exception& e) {
                    CVEDIX_ERROR(cvedix_utils::string_format("[%s] JSON transformation failed: %s", 
                        node_name.c_str(), e.what()));
                    // Continue with original message if transformation fails
                } catch (...) {
                    CVEDIX_ERROR(cvedix_utils::string_format("[%s] JSON transformation failed with unknown error", 
                        node_name.c_str()));
                    // Continue with original message if transformation fails
                }
            }
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] format_msg failed: %s", 
                node_name.c_str(), e.what()));
            msg = "";
        } catch (...) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] format_msg failed with unknown error", 
                node_name.c_str()));
            msg = "";
        }
    }
    
    void cvedix_mqtt_broker_node::broke_msg(const std::string& msg) {
        if (msg.empty()) {
            return;
        }
        
        if (mqtt_publisher != nullptr) {
            try {
                // Call user-provided MQTT publisher function
                mqtt_publisher(msg);
            } catch (const std::exception& e) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] MQTT publisher function failed: %s", 
                    node_name.c_str(), e.what()));
            } catch (...) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] MQTT publisher function failed with unknown error", 
                    node_name.c_str()));
            }
        } else {
            // If no publisher is set, just log the message (for debugging)
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] MQTT publisher not set, message: %s", 
                node_name.c_str(), msg.substr(0, 200).c_str()));
        }
    }
}

