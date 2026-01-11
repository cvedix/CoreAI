#pragma once

#include "cvedix_track_node.h"
#include "bot_sort/BoTSORT.h"

namespace cvedix_nodes {

    class cvedix_botsort_track_node: public cvedix_track_node{
        private:
            std::map<int, std::unique_ptr<BoTSORT>> channel_trackers;
            std::string tracker_config_path;
            std::string gmc_config_path;
            std::string reid_config_path;
            std::string reid_onnx_model_path;

        protected:
            // fill track_ids using bot sort algo
            virtual void track(int channel_index, const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta,
                        const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        const std::vector<std::vector<float>>& target_embeddings, 
                        std::vector<int>& track_ids) override;

        public:
            cvedix_botsort_track_node(std::string node_name, 
                                cvedix_track_for track_for = cvedix_track_for::NORMAL,
                                std::string tracker_config_path = "",
                                std::string gmc_config_path = "",
                                std::string reid_config_path = "",
                                std::string reid_onnx_model_path = ""
                            );
            virtual ~cvedix_botsort_track_node();
    };
}