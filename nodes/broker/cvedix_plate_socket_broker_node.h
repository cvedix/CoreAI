/**
 * @file cvedix_plate_socket_broker_node.h
 * @brief UDP socket broker for license plate recognition (LPR) results
 * 
 * Exports license plate detection results via UDP. Saves plate crop images
 * and filters duplicates by text similarity.
 * 
 * @section lpr_overview Features
 * - Plate text and color extraction
 * - Cropped plate image saving
 * - Duplicate filtering by similarity
 * - Track-based filtering
 * 
 * @section lpr_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_plate_socket_broker_node>(
 *     "lpr_broker",
 *     "192.168.1.50", 8000,
 *     "/data/plates",  // save directory
 *     100, 0,          // min crop size
 *     cvedix_broke_for::NORMAL,
 *     true             // only tracked plates
 * );
 * broker->attach_to({plate_detector_node});
 * @endcode
 * 
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

// light weight socket support
#include "cvedix/third_party/kissnet/kissnet.hpp"

namespace cvedix_nodes {

    /**
     * @brief License plate recognition (LPR) UDP broker
     * 
     * Exports plate text/color, saves crop images, filters duplicates.
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_plate_socket_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Directory for plate images
        std::string plates_dir = "plate_images";
        /// @brief Minimum plate width to process
        int min_crop_width = 50;
        /// @brief Minimum plate height to process
        int min_crop_height = 50;
        /// @brief Only process tracked plates
        bool only_for_tracked = false;
        
        /// @brief Minimum tracked frames before export
        int min_tracked_frames = 25;

        /// @brief Destination IP address
        std::string des_ip = "";
        /// @brief Destination port
        int des_port = 0;

        /// @brief UDP socket writer
        kissnet::udp_socket udp_writer;

        /// @brief Already exported track IDs per channel
        std::map<int, std::vector<int>> all_broked_ids;
        /// @brief Already exported plate texts per channel (for similarity filter)
        std::map<int, std::vector<std::string>> all_broked_texts;

    protected:
        /**
         * @brief Format plate data to message
         * @param meta Frame meta with plate targets
         * @param[out] msg Output message
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief Send via UDP socket
         * @param msg Message to send
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param des_ip Destination IP
         * @param des_port Destination port
         * @param plates_dir Plate images directory
         * @param min_crop_width Minimum plate width
         * @param min_crop_height Minimum plate height
         * @param broke_for Target type
         * @param only_for_tracked Only tracked plates
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         */
        cvedix_plate_socket_broker_node(std::string node_name, 
                                std::string des_ip = "",
                                int des_port = 0,
                                std::string plates_dir = "plate_images",
                                int min_crop_width = 100,
                                int min_crop_height = 0,
                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                bool only_for_tracked = true, 
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_plate_socket_broker_node();
    };
}