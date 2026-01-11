/**
 * @file cvedix_dsort_track_node.h
 * @brief DeepSORT tracking with appearance features
 * 
 * SORT with deep appearance feature matching for re-identification.
 * 
 * @see cvedix_track_node Base class
 * @see cvedix_sort_track_node SORT implementation
 */

#pragma once
#include "cvedix_track_node.h"

namespace cvedix_nodes {
    /**
     * @brief DeepSORT tracker node
     */
    class cvedix_dsort_track_node: public cvedix_track_node
    {

    private:
        /* config data for deep sort algo*/
    protected:
        // fill track_ids using deep sort algo
        virtual void track(int channel_index, const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta,
                        const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        const std::vector<std::vector<float>>& target_embeddings, 
                        std::vector<int>& track_ids) override;
    public:
        cvedix_dsort_track_node(std::string node_name, cvedix_track_for track_for = cvedix_track_for::NORMAL);
        virtual ~cvedix_dsort_track_node();
    };
}
