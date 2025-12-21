/**
 * @file cvedix_frame_fusion_node.h
 * @brief Multi-camera frame fusion using calibration
 * 
 * Fuses frames from 2 channels using homography transform.
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Frame fusion node (2 channels)
     */
    class cvedix_frame_fusion_node: public cvedix_node
    {

    private:
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> tmp_des = nullptr;
        cv::Mat trans_mat;
        int src_channel_index = 0;
        int des_channel_index = 1;

        void fuse(cv::Mat& src_canvas, cv::Mat& des_canvas);
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_frame_fusion_node(std::string node_name, 
                            std::vector<cvedix_objects::cvedix_point> src_points,   // 4 calibration points of the source frame
                            std::vector<cvedix_objects::cvedix_point> des_points,   // 4 calibration points of the destination frame
                            int src_channel_index = 0, 
                            int des_channel_index = 1);
        ~cvedix_frame_fusion_node();
    };
}