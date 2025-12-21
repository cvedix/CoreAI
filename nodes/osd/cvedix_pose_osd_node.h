/**
 * @file cvedix_pose_osd_node.h
 * @brief OSD for human pose estimation
 * 
 * Draws skeleton keypoints and limb connections (BODY25, COCO, MPI, YOLOv8).
 */

#pragma once

#include <random>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Pose estimation OSD visualization
     */
    class cvedix_pose_osd_node: public cvedix_node
    {

    private:
        // pose pairs for PAFs
        const std::map<cvedix_objects::cvedix_pose_type, std::vector<std::pair<int,int>>> posePairs_map = {
            {cvedix_objects::cvedix_pose_type::body_25, {{1,8}, {1,2}, {1,5}, {2,3}, {3,4}, {5,6}, {6,7}, {8,9}, 
                                                {9,10}, {10,11}, {8,12}, {12,13}, {13,14}, {1,0}, {0,15}, {15,17}, {0,16}, {16,18}, 
                                                {14,19}, {19,20}, {14,21}, {11,22}, {22,23}, {11,24}}},
            {cvedix_objects::cvedix_pose_type::coco, {{1,2}, {1,5}, {2,3}, {3,4}, {5,6}, {6,7},
                                            {1,8}, {8,9}, {9,10}, {1,11}, {11,12}, {12,13},
                                            {1,0}, {0,14}, {14,16}, {0,15}, {15,17}, {2,16},
                                            {5,17}}},
            {cvedix_objects::cvedix_pose_type::mpi_15, {{0,1}, {1,2}, {2,3}, {3,4}, {1,5}, {5,6}, {6,7}, {1,14}, {14,8}, {8,9}, {9,10}, {14,11}, {11,12}, {12,13}, {0, 2}, {0, 5}}},
            {cvedix_objects::cvedix_pose_type::hand, std::vector<std::pair<int,int>>()},
            {cvedix_objects::cvedix_pose_type::face, std::vector<std::pair<int,int>>()},
            {cvedix_objects::cvedix_pose_type::yolov8_pose_17, {{0, 1}, {0, 2},  {0, 5}, {0, 6},  {1, 2},   {1, 3},   {2, 4},   {5, 6},   {5, 7},  {5, 11},
                                            {6, 8}, {6, 12}, {7, 9}, {8, 10}, {11, 12}, {11, 13}, {12, 14}, {13, 15}, {14, 16}}}
        };

        void populateColorPalette(std::vector<cv::Scalar>& colors, int nColors) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis1(64, 200);
            std::uniform_int_distribution<> dis2(100, 255);
            std::uniform_int_distribution<> dis3(100, 255);

            for(int i = 0; i < nColors; ++i){
                colors.push_back(cv::Scalar(dis1(gen),dis2(gen),dis3(gen)));
            }
        }
        std::vector<cv::Scalar> colors;
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_pose_osd_node(std::string node_name);
        ~cvedix_pose_osd_node();
    };

}