/**
 * @file cvedix_ocsort_track_node.h
 * @brief OC Sort tracking algorithm implementation
 *
 *
 * @see cvedix_track_node Base class
 */

#pragma once

#include "cvedix_track_node.h"
#include "oc_sort/OCSort.h"

namespace cvedix_nodes {

    class cvedix_ocsort_track_node: public cvedix_track_node
    {
        private:
            std::map<int, ocsort::OCSort> channel_trackers;
            float det_thresh;
            int   max_age;
            int   min_hits;
            float iou_threshold;
            int   delta_t;
            std::string asso_func;
            float inertia;
            bool  use_byte;

        protected:
            // fill track_ids using oc sort algo
            virtual void track(int channel_index, const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta,
                        const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        const std::vector<std::vector<float>>& target_embeddings, 
                        std::vector<int>& track_ids) override;
        public:
            cvedix_ocsort_track_node(std::string node_name, 
                                cvedix_track_for track_for = cvedix_track_for::NORMAL,
                                float det_thresh = 0.25,
                                int   max_age = 30,
                                int   min_hits = 3,
                                float iou_threshold = 0.3,
                                int   delta_t = 3,
                                std::string asso_func = "iou",
                                float inertia = 0.2,
                                bool  use_byte = false);
            virtual ~cvedix_ocsort_track_node();
    };

} // namespace cvedix_nodes