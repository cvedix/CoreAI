#include "cvedix_dsort_track_node.h"

namespace cvedix_nodes {
        
    cvedix_dsort_track_node::cvedix_dsort_track_node(std::string node_name, 
                                            cvedix_track_for track_for):
                                            cvedix_track_node(node_name, track_for) {
        this->initialized();
    }
    
    cvedix_dsort_track_node::~cvedix_dsort_track_node() {
        deinitialized();
    }

    void cvedix_dsort_track_node::track(int channel_index, const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                const std::vector<std::vector<float>>& target_embeddings, 
                std::vector<int>& track_ids) {
        // fill track_ids according to target_rects & target_embeddings
        // deep sort logic here ...         
    }
}