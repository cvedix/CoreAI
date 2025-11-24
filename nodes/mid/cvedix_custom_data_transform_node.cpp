#include "cvedix_custom_data_transform_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"

namespace cvedix_nodes {
    
    cvedix_custom_data_transform_node::cvedix_custom_data_transform_node(
        std::string node_name,
        std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> transform_func)
        : cvedix_node(node_name),
          transform_func(transform_func) {
        
        // Call initialized() after all members are set
        this->initialized();
        
        if (transform_func == nullptr) {
            CVEDIX_WARN(cvedix_utils::string_format("[%s] Transform function not set. Frames will be forwarded without modification.", 
                node_name.c_str()));
        }
    }
    
    cvedix_custom_data_transform_node::~cvedix_custom_data_transform_node() {
        // Call deinitialized() before destruction
        this->deinitialized();
    }
    
    void cvedix_custom_data_transform_node::set_transform_func(
        std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> func) {
        transform_func = func;
    }
    
    std::function<std::shared_ptr<cvedix_objects::cvedix_frame_meta>(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta>)> 
        cvedix_custom_data_transform_node::get_transform_func() const {
        return transform_func;
    }
    
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_custom_data_transform_node::handle_frame_meta(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        
        if (meta == nullptr) {
            return nullptr;
        }
        
        // If no transform function is set, forward meta as-is
        if (transform_func == nullptr) {
            return meta;
        }
        
        // Apply custom transformation
        try {
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> transformed_meta = transform_func(meta);
            
            // If transform function returns nullptr, drop this frame
            if (transformed_meta == nullptr) {
                return nullptr;
            }
            
            // Return transformed meta to be forwarded to next nodes
            return transformed_meta;
            
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Transform function failed: %s. Forwarding original frame.", 
                node_name.c_str(), e.what()));
            // On error, forward original meta
            return meta;
        } catch (...) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Transform function failed with unknown error. Forwarding original frame.", 
                node_name.c_str()));
            // On error, forward original meta
            return meta;
        }
    }
}

