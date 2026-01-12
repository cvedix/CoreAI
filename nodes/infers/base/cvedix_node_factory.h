#pragma once

#include <string>
#include <functional>
#include <map>
#include <memory>
#include "cvedix_infer_node.h" 

namespace cvedix_nodes {

    class cvedix_infer_node; // Forward

    using NodeCreator = std::function<std::shared_ptr<cvedix_infer_node>(
        const std::string& name,
        const std::string& model_path,
        const std::string& config)>;

    class cvedix_node_factory {
    public:
        static cvedix_node_factory& get_instance();

        void register_node(const std::string& type, NodeCreator creator);
        
        std::shared_ptr<cvedix_infer_node> create_node(
            const std::string& type,
            const std::string& name,
            const std::string& model_path,
            const std::string& config = "");

    private:
        std::map<std::string, NodeCreator> creators;
    };

    // Helper class for static registration
    class cvedix_node_registrar {
    public:
        cvedix_node_registrar(const std::string& type, NodeCreator creator) {
            cvedix_node_factory::get_instance().register_node(type, creator);
        }
    };
    
    // Macro for plugins to use
    #define CVEDIX_REGISTER_NODE(TYPE, EXECUTOR_CLASS) \
        static cvedix_nodes::cvedix_node_registrar register_##EXECUTOR_CLASS( \
            TYPE, [](const std::string& name, const std::string& model, const std::string& cfg) { \
                return std::make_shared<EXECUTOR_CLASS>(name, model, 0.5f, 0.45f, 640, 640, 80, "", 0); \
            });

}
