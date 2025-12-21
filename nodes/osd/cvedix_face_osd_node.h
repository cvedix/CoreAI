/**
 * @file cvedix_face_osd_node.h
 * @brief OSD for face detection/recognition results
 * 
 * Draws cvedix_frame_face_target (bounding boxes, keypoints, names).
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Face OSD visualization
     */
    class cvedix_face_osd_node: public cvedix_node
    {

    private:
        /* data */
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_face_osd_node(std::string node_name);
        ~cvedix_face_osd_node();
    };

}