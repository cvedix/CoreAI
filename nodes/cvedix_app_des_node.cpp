
#include "cvedix_app_des_node.h"

namespace cvedix_nodes {
        
    cvedix_app_des_node::cvedix_app_des_node(std::string node_name, 
                                        int channel_index): 
                                        cvedix_des_node(node_name, channel_index) {
        this->initialized();
    }
    
    cvedix_app_des_node::~cvedix_app_des_node() {
        deinitialized();
    }
    
    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_app_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));

            invoke_app_des_result_hooker(meta);

            // for general works defined in base class
            return cvedix_des_node::handle_frame_meta(meta);
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_app_des_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
            invoke_app_des_result_hooker(meta);
            return cvedix_des_node::handle_control_meta(meta);
    }

    void cvedix_app_des_node::set_app_des_result_hooker(cvedix_app_des_result_hooker app_des_result_hooker) {
        this->app_des_result_hooker = app_des_result_hooker;
    }

    void cvedix_app_des_node::invoke_app_des_result_hooker(std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
        if (app_des_result_hooker) {
            app_des_result_hooker(node_name, meta);
        }
    }
}