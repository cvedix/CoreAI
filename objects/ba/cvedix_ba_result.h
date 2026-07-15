/**
 * @file cvedix_ba_result.h
 * @brief Behavior analysis result
 * 
 * Result from BA nodes: crossline, stop, jam, crowding detection, etc.
 * Contains event metadata, involved target details (bbox, crop), 
 * zone/line identification, and timestamp.
 */

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <random>
#include <cstdio>
#include <thread>
#include <opencv2/core.hpp>
#include "cvedix/objects/shapes/cvedix_point.h"

namespace cvedix_objects {

    // Forward declarations
    class cvedix_frame_target;

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
        PARKING = 0b1000000000000000,
        RED_LIGHT = 0b10000000000000000,
        ILLEGAL_TURN = 0b100000000000000000,
        WRONG_WAY = 0b1000000000000000000,
        LANE_VIOLATION = 0b10000000000000000000,
        STOP_LINE = 0b100000000000000000000,
        NO_ENTRY = 0b1000000000000000000000,
        ILLEGAL_UTURN = 0b10000000000000000000000,
        HELMET = 0b100000000000000000000000,
        INTRUSION_START = 0b1000000000000000000000000,
        INTRUSION_END = 0b10000000000000000000000000,
        LOITERING_END = 0b100000000000000000000000000
    };

    /** @brief BA object move direction types */
    enum class cvedix_ba_direct_type {
        IN = 1,
        OUT = 2,
        BOTH = 3
    };

    /**
     * @brief Detail info for a single target involved in the BA event
     * 
     * Contains bounding box, classification, and optional crop image.
     * Populated by populate_target_details() or manually by BA nodes.
     */
    struct involved_target_info {
        /// @brief Bounding box in pixel coordinates
        int x = 0, y = 0, width = 0, height = 0;
        /// @brief Normalized location (0.0-1.0 relative to frame size)
        double location_x = 0, location_y = 0, location_w = 0, location_h = 0;
        /// @brief Track ID (int, internal use)
        int track_id = -1;
        /// @brief UUID-style tracking reference ID for customer integration
        std::string ref_tracking_id = "";
        /// @brief Primary class ID from detector
        int class_id = -1;
        /// @brief Primary confidence score 
        float score = 0.0f;
        /// @brief Primary class label (e.g., "Person", "Vehicle")
        std::string object_class = "";
        /// @brief Cropped image of the target (optional, empty if not enabled)
        cv::Mat crop;
    };

    /**
     * @brief Behavior analysis result
     * 
     * Contains event type, involved targets with bbox/crop, 
     * zone/line identification, timestamp, and recording info.
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

        // ===== Enhanced fields for customer integration =====

        /// @brief Unique event ID (UUID v4 format)
        std::string event_id = "";

        /// @brief Event timestamp in epoch milliseconds (0 = not set)
        double event_timestamp_ms = 0;

        /// @brief ISO 8601 system datetime (e.g., "2026-03-23T14:47:06Z")
        std::string system_datetime = "";

        /// @brief System timestamp in epoch milliseconds
        double system_timestamp = 0;

        /// @brief Event duration in milliseconds (used for end events like intrusion-end)
        double event_duration_ms = 0;

        /// @brief Type of region: "area" or "line"
        std::string region_type = "";

        /// @brief UUID of the region/area/line from config
        std::string region_id = "";

        /// @brief User-defined name for the region
        std::string region_name = "";

        /// @brief Index of the region in its configuration (0-based, -1 = not set)
        int region_index = -1;

        /// @brief Detailed info for each involved target
        std::vector<involved_target_info> involve_target_details;

        // ===== Constructors =====

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

        // ===== Helper methods =====

        /**
         * @brief Populate involve_target_details from targets in current frame
         * 
         * Looks up each track_id in involve_target_ids_in_frame, extracts
         * bbox, class_id, score, label, and optionally crops the target region.
         * 
         * @param targets_in_frame All targets from frame_meta->targets
         * @param frame The video frame (for cropping)
         * @param include_crops If true, crop each target's bbox from the frame
         */
        void populate_target_details(
            const std::vector<std::shared_ptr<cvedix_frame_target>>& targets_in_frame,
            const cv::Mat& frame,
            bool include_crops = false);

        /**
         * @brief Set event timestamp to current system time (epoch ms + ISO 8601)
         * Also generates event_id if empty
         */
        void stamp_now();

        /**
         * @brief Convert ba_type enum to human-readable string
         */
        static std::string ba_type_to_string(cvedix_ba_type t);

        /**
         * @brief Generate a UUID v4-like string
         * @return UUID string (e.g., "d41de3b2-3682-4c6d-96c3-6b506d5d6c7b")
         */
        static std::string generate_uuid();
            static std::string generate_tracking_ref_id(int channel_index, int track_id);
    };

}