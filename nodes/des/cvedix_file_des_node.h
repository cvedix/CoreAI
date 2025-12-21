/**
 * @file cvedix_file_des_node.h
 * @brief Video file recording destination node
 * 
 * Records pipeline output to MP4 video files with automatic file splitting.
 * 
 * @section file_des_features Features
 * - GStreamer-based encoding (x264enc default)
 * - Automatic file splitting by duration
 * - Custom resolution and bitrate
 * - Optional OSD overlay
 * 
 * @section file_des_usage Usage
 * @code
 * auto file_des = std::make_shared<cvedix_file_des_node>(
 *     "recorder", 0,
 *     "/recordings",      // save directory
 *     "cam1_",            // filename prefix
 *     5,                  // 5 minutes per file
 *     {1920, 1080},       // resolution
 *     2048,               // bitrate
 *     true                // OSD enabled
 * );
 * file_des->attach_to({pipeline_node});
 * @endcode
 * 
 * @see cvedix_des_node Base class
 */

#pragma once

#include <iostream>
#include <memory>
#include <chrono>
// compile tips:
// remove experimental/ if gcc >= 8.0
#include <experimental/filesystem>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "cvedix/nodes/common/cvedix_des_node.h"
#include "cvedix/objects/cvedix_frame_meta.h"
#include "cvedix/objects/cvedix_control_meta.h"
#include "cvedix/utils/cvedix_utils.h"


namespace cvedix_nodes {

    /**
     * @brief Video file recording destination node
     * 
     * Records to MP4 with automatic file splitting and encoding.
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_file_des_node: public cvedix_des_node {
    private:
        /// @brief GStreamer pipeline template
        std::string gst_template = "appsrc ! videoconvert ! %s bitrate=%d ! mp4mux ! filesink location=%s";
        /// @brief OpenCV video writer
        cv::VideoWriter file_writer;

        /// @brief Frames already recorded in current file
        int frames_already_record = -1;
        /// @brief Frames needed for current recording session
        int frames_need_record = 0;

        /**
         * @brief Generate new filename with timestamp
         * @return Full path: save_dir/name_prefix_timestamp.mp4
         */
        std::string get_new_file_name();

    protected:
        /**
         * @brief Record frame to file
         * @param meta Frame to record
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 

        /**
         * @brief Handle control meta (recording commands)
         * @param meta Control meta
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param channel_index Channel index
         * @param save_dir Directory for saved files
         * @param name_prefix Filename prefix
         * @param max_duration_for_single_file Max file duration (minutes)
         * @param resolution_w_h Output resolution
         * @param bitrate Video bitrate (kbps)
         * @param osd Enable OSD overlay
         * @param gst_encoder_name GStreamer encoder (default: x264enc)
         */
        cvedix_file_des_node(std::string node_name, 
                        int channel_index, 
                        std::string save_dir,
                        std::string name_prefix = "",
                        int max_duration_for_single_file = 2,
                        cvedix_objects::cvedix_size resolution_w_h = {},
                        int bitrate = 1024,
                        bool osd = true,
                        std::string gst_encoder_name = "x264enc");

        /// @brief Destructor
        ~cvedix_file_des_node();

        /// @brief Save directory
        std::string save_dir;
        /// @brief Filename prefix
        std::string name_prefix;
        /// @brief Max file duration (minutes)
        int max_duration_for_single_file;
        /// @brief Output resolution
        cvedix_objects::cvedix_size resolution_w_h;
        /// @brief Video bitrate
        int bitrate;
        /// @brief OSD enabled
        bool osd;
        /// @brief GStreamer encoder name
        std::string gst_encoder_name = "x264enc";
    };

}