/**
 * @file cvedix_feature_encoder_node.h
 * @brief Generic feature encoder for embedding extraction
 * 
 * Secondary inference node for extracting feature embeddings from detected objects.
 * Updates `embeddings` field of `cvedix_frame_target`.
 * 
 * @see cvedix_secondary_infer_node Base class
 */

#pragma once

#include "base/cvedix_secondary_infer_node.h"

namespace cvedix_nodes {
    /**
     * @brief Generic feature encoder node
     * 
     * Extracts embeddings for ReID/tracking from detected targets.
     */
    class cvedix_feature_encoder_node: public cvedix_secondary_infer_node
    {

    private:
        /* data */
    public:
        cvedix_feature_encoder_node(std::string node_name, std::string model_path);
        ~cvedix_feature_encoder_node();
    };

}