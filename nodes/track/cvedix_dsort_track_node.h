#pragma once
#include "cvedix_track_node.h"

namespace cvedix_nodes {
    // track node using deep sort
    class cvedix_dsort_track_node: public cvedix_track_node
    {
    private:
        /* config data for deep sort algo*/
    protected:
        // fill track_ids using deep sort algo
        virtual void track(int channel_index, const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        const std::vector<std::vector<float>>& target_embeddings, 
                        std::vector<int>& track_ids) override;
    public:
        cvedix_dsort_track_node(std::string node_name, cvedix_track_for track_for = cvedix_track_for::NORMAL);
        virtual ~cvedix_dsort_track_node();
    };
}
