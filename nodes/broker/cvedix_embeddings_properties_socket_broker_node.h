/**
 * @file cvedix_embeddings_properties_socket_broker_node.h
 * @brief UDP socket broker for embeddings and target properties
 * 
 * This node exports target embeddings and properties via UDP socket,
 * along with saving cropped images for later similarity search.
 * 
 * @section embed_overview Overview
 * When targets are detected with embeddings (e.g., from ReID or face recognition),
 * this node:
 * 1. Saves cropped images of each target to disk
 * 2. Sends embedding vectors and properties via UDP
 * 3. Filters by minimum size and tracking status
 * 
 * @section embed_usecase Use Cases
 * - Building embedding databases for similarity search
 * - Real-time feature export for external processing
 * - Collecting training data from live video
 * 
 * @section embed_usage Usage Example
 * @code
 * auto broker = std::make_shared<cvedix_embeddings_properties_socket_broker_node>(
 *     "embed_broker",
 *     "192.168.1.50",      // destination IP
 *     6000,                // destination port
 *     "/data/crops",       // cropped images directory
 *     64, 64,              // min crop size (w, h)
 *     cvedix_broke_for::NORMAL,
 *     true                 // only for tracked targets
 * );
 * broker->attach_to({reid_node});
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
     * @brief UDP socket broker for target embeddings and properties
     * 
     * Exports embedding vectors and target properties, saves cropped images
     * for building searchable databases.
     * 
     * @note Currently supports cvedix_frame_target only (broke_for::NORMAL)
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_embeddings_properties_socket_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Directory to save cropped target images
        std::string cropped_dir = "cropped_images";
        /// @brief Minimum target width for processing (skip smaller)
        int min_crop_width = 50;
        /// @brief Minimum target height for processing (skip smaller)
        int min_crop_height = 50;
        /// @brief Only process tracked targets (track_id != -1)
        bool only_for_tracked = false;
        
        /// @brief Minimum frames tracked before exporting (if only_for_tracked=true)
        int min_tracked_frames = 25;

        /// @brief Destination IP address for UDP packets
        std::string des_ip = "";
        /// @brief Destination port number
        int des_port = 0;

        /// @brief UDP socket writer (kissnet library)
        kissnet::udp_socket udp_writer;

        /// @brief Tracks exported targets per channel: channel → list of track_ids
        std::map<int, std::vector<int>> all_broked;

    protected:
        /**
         * @brief Format embeddings and properties to message
         * @param meta Frame meta containing targets with embeddings
         * @param[out] msg Output message string
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief Send message via UDP socket
         * @param msg Message to send
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Unique node identifier
         * @param des_ip Destination IP address
         * @param des_port Destination UDP port
         * @param cropped_dir Directory for cropped images (default: "cropped_images")
         * @param min_crop_width Minimum target width to process (default: 50)
         * @param min_crop_height Minimum target height to process (default: 50)
         * @param broke_for Target type (default: NORMAL)
         * @param only_for_tracked Only export tracked targets (default: false)
         * @param broking_cache_warn_threshold Queue warning threshold (default: 50)
         * @param broking_cache_ignore_threshold Queue ignore threshold (default: 200)
         */
        cvedix_embeddings_properties_socket_broker_node(std::string node_name, 
                                std::string des_ip = "",
                                int des_port = 0,
                                std::string cropped_dir = "cropped_images",
                                int min_crop_width = 50,
                                int min_crop_height = 50,
                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                bool only_for_tracked = false, 
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_embeddings_properties_socket_broker_node();
    };
}