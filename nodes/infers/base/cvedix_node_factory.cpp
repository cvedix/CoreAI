#include "cvedix_node_factory.h"
#include "cvedix/utils/logger/cvedix_logger.h"

namespace cvedix_nodes {

    cvedix_node_factory& cvedix_node_factory::get_instance() {
        static cvedix_node_factory instance;
        return instance;
    }

    void cvedix_node_factory::register_node(const std::string& type, NodeCreator creator) {
        creators[type] = creator;
        CVEDIX_INFO(cvedix_utils::string_format("[NodeFactory] Registered node execution type: %s", type.c_str()));
    }

    std::shared_ptr<cvedix_infer_node> cvedix_node_factory::create_node(
        const std::string& type,
        const std::string& name,
        const std::string& model_path,
        const std::string& config) {
        
        if (creators.find(type) != creators.end()) {
            return creators[type](name, model_path, config);
        }
        
        CVEDIX_ERROR(cvedix_utils::string_format("[NodeFactory] Unknown node type: %s", type.c_str()));
        return nullptr;
    }

}
