/**
 * @file cvedix_osd_node.h
 * @brief On-Screen Display node for drawing detection results
 * 
 * Base OSD node for displaying cvedix_frame_target on frames.
 * Also supports static rendering of BA zone/line geometry on every frame.
 * Supports Chinese/Unicode fonts via FreeType.
 * 
 * @section osd_purpose Purpose
 * - Debug: Visual verification of inference results
 * - Demo: Screen casting to showcase product capabilities
 * - BA Zones: Always-on visualization of crosslines and crowding zones
 */

#pragma once

#include <map>
#include <vector>
#include <string>
#include <opencv2/freetype.hpp>
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_line.h"

namespace cvedix_nodes {

    /** @brief OSD configuration options */
    typedef struct cvedix_osd_node_option
    {
        int aaa = 0;
    } cvedix_osd_option;

    /** @brief Configuration for a crossline display */
    struct osd_line_config {
        cvedix_objects::cvedix_line line;
        cv::Scalar color = cv::Scalar(0, 255, 0);
        std::string name;

        osd_line_config() = default;
        osd_line_config(const cvedix_objects::cvedix_line &l,
                        const cv::Scalar &c = cv::Scalar(0, 255, 0),
                        const std::string &n = "")
            : line(l), color(c), name(n) {}
    };

    /** @brief Configuration for a crowding zone display */
    struct osd_zone_config {
        std::vector<cvedix_objects::cvedix_point> roi;
        cv::Scalar color = cv::Scalar(0, 200, 0);
        std::string name;

        osd_zone_config() = default;
        osd_zone_config(const std::vector<cvedix_objects::cvedix_point> &r,
                        const cv::Scalar &c = cv::Scalar(0, 200, 0),
                        const std::string &n = "")
            : roi(r), color(c), name(n) {}
    };
    
    /**
     * @brief Base OSD node for target visualization and BA geometry
     */
    class cvedix_osd_node: public cvedix_node {

    private:
        // support chinese font
        cv::Ptr<cv::freetype::FreeType2> ft2;

        // Static BA geometry for always-on drawing
        std::vector<osd_line_config> _static_lines;
        std::vector<osd_zone_config> _static_zones;

    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

        void draw_static_lines(cv::Mat &canvas);
        void draw_static_zones(cv::Mat &canvas);

    public:
        cvedix_osd_node(std::string node_name, std::string font = "");
        ~cvedix_osd_node();

        /**
         * @brief Set crosslines to draw on every frame
         * @param lines Vector of line configs with coordinates, color, name
         */
        void set_static_lines(const std::vector<osd_line_config> &lines);

        /**
         * @brief Set crowding zones to draw on every frame
         * @param zones Vector of zone configs with ROI polygon, color, name
         */
        void set_static_zones(const std::vector<osd_zone_config> &zones);
    };

}