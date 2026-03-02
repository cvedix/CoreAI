/**
 * @file cvedix_ba_area_crowding_osd_node.h
 * @brief OSD for crowding behavior analysis
 *
 * Draws ROI rectangle and highlights targets when crowding is detected.
 */

#pragma once

#include <map>
#include <opencv2/freetype.hpp>
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_point.h"

namespace cvedix_nodes {
    /**
     * @brief Crowding BA visualization
     *
     * Notes:
     * - The BA node currently emits a BA result with label "crowding" (and
     *   uses existing BA enums). This OSD watches `meta->ba_results` for
     *   results whose `ba_label` == "crowding" and renders the provided
     *   `involve_region_in_frame` and `involve_target_ids_in_frame`.
     */
    class cvedix_ba_area_crowding_osd_node: public cvedix_node
    {

    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;

        // per-channel state
        std::map<int, std::vector<cvedix_objects::cvedix_point>> all_rois; // last seen roi polygon
        std::map<int, bool> all_crowding;                   // channel -> crowding flag
        std::map<int, std::vector<int>> all_involve_ids;    // channel -> highlighted target ids

    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_ba_area_crowding_osd_node(std::string node_name,  std::string font = "");
        ~cvedix_ba_area_crowding_osd_node();
    };
}
