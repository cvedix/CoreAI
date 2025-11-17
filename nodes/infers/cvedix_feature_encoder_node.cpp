
#include "cvedix_feature_encoder_node.h"


namespace cvedix_nodes {
    
    cvedix_feature_encoder_node::cvedix_feature_encoder_node(std::string node_name, std::string model_path):
                                                    cvedix_secondary_infer_node(node_name, model_path) {
        this->initialized();
    }
    
    cvedix_feature_encoder_node::~cvedix_feature_encoder_node() {
        deinitialized();        
    } 
}