#pragma once

#include <map>
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/cvedix_image_record_control_meta.h"
#include "cvedix/objects/cvedix_video_record_control_meta.h"


namespace cvedix_nodes {

    class cvedix_ba_line_counting : public cvedix_node
    {
        
        private:

            std::map<int, std::vector<int>> all_line_cross_counting;

            std::map<int, std::vector<cvedix_objects::cvedix_ba_direct_type>> all_line_detect_directions;

            std::map<int, std::vector<cvedix_objects::cvedix_line>> all_line_settings;

            /// @brief Whether to trigger image recording on crossline event
            bool need_record_image;
            /// @brief Whether to trigger video recording on crossline event
            bool need_record_video;
        protected:
            /**
            * @brief Process frame meta for crossline detection
            * @param meta Frame meta with tracked targets
            * @return Processed meta (may include record control metas)
            */
            virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        public:
            /**
             * @brief Constructor
             */
            cvedix_ba_line_counting(std::string node_name,
                                std::map<int, std::vector<cvedix_objects::cvedix_line>> all_line_settings,
                                std::map<int, std::vector<cvedix_objects::cvedix_ba_direct_type>> all_line_detect_directions,
                                bool need_record_image = false,
                                bool need_record_video = false);
    
            /**
             * @brief Destructor
             * 
             */
            ~cvedix_ba_line_counting();

    };

} //namespace cvedix_nodes