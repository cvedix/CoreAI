
#pragma once

#include "base/cvedix_secondary_infer_node.h"

namespace cvedix_nodes {
    // common feature encoder for image feature extraction.
    // used for feature extraction, update embeddings of cvedix_frame_target.
    class cvedix_feature_encoder_node: public cvedix_secondary_infer_node
    {
    private:
        /* data */
    public:
        cvedix_feature_encoder_node(std::string node_name, std::string model_path);
        ~cvedix_feature_encoder_node();
    };

}