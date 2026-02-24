/**
 * @file cvedix_ba_result.h
 * @brief Behavior analysis result
 * 
 * Result from BA nodes: crossline, stop, jam detection.
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include "cvedix/objects/shapes/cvedix_point.h"

namespace cvedix_objects {
    /** @brief BA event types */
    enum class cvedix_ba_type {
        NONE = 0b00000000,
        CROSSLINE = 0b00000001,
        STOP = 0b00000010,
        UNSTOP = 0b00000100,
        JAM = 0b00001000,
        UNJAM = 0b00010000,
        AREA_ENTER = 0b00100000,
        AREA_EXIT = 0b01000000,
        SPEED = 0b10000000,
        DIRECTION = 0b100000000,
        DWELL = 0b1000000000,
        QUEUE = 0b10000000000,
        FALL = 0b100000000000,
        CROWDING = 0b1000000000000,
        LOITERING = 0b10000000000000,
        FIGHT = 0b100000000000000,
        PARKING = 0b1000000000000000
    };

    /** @brief BA object move direction types */
    enum class cvedix_ba_direct_type {
        IN = 1,
        OUT = 2,
        BOTH = 3
    };

    /**
     * @brief Behavior analysis result
     */
    class cvedix_ba_result
    {

    private:
        /* data */
    public:
        // type
        cvedix_ba_type type;
        // target ids which involved for this ba result, empty allowed.
        std::vector<int> involve_target_ids_in_frame;
        // region (or single line) involved for this ba result, empty allowed.
        std::vector<cvedix_objects::cvedix_point> involve_region_in_frame;

        // channel index of this ba result
        int channel_index;
        // frame index of this ba result
        int frame_index;

        // name of ba
        std::string ba_label = "not specified";

        // record image name if exist
        std::string record_image_name = "";
        // record video name if exist
        std::string record_video_name = "";

        cvedix_ba_result(cvedix_ba_type type, 
                    int channel_index,
                    int frame_index,
                    std::vector<int> involve_target_ids_in_frame, 
                    std::vector<cvedix_objects::cvedix_point> involve_region_in_frame,
                    std::string ba_label = "not specified",
                    std::string record_image_name = "",
                    std::string record_video_name = "");
        ~cvedix_ba_result();

        // get description for ba result
        virtual std::string to_string();

        // clone myself
        std::shared_ptr<cvedix_ba_result> clone();
    };

}