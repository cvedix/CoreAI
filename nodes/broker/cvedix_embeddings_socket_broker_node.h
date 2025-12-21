/**
 * @file cvedix_embeddings_socket_broker_node.h
 * @brief UDP socket broker for target embeddings (embeddings only, no properties)
 * 
 * This node exports target embeddings via UDP socket and saves cropped images.
 * Unlike cvedix_embeddings_properties_socket_broker_node, this version exports
 * only embeddings without additional target properties.
 * 
 * @section embed_only_overview Overview
 * When targets are detected with embeddings (from ReID or face recognition):
 * 1. Saves cropped images of qualifying targets
 * 2. Sends embedding vectors via UDP
 * 3. Filters by size and tracking status
 * 
 * @section embed_only_vs Comparison
 * | Feature | embeddings_socket | embeddings_properties_socket |
 * |---------|-------------------|------------------------------|
 * | Embeddings | ✓ | ✓ |
 * | Properties | ✗ | ✓ |
 * | Message size | Smaller | Larger |
 * | Use case | Similarity search | Full feature export |
 * 
 * @section embed_only_usage Usage Example
 * @code
 * auto broker = std::make_shared<cvedix_embeddings_socket_broker_node>(
 *     "embed_broker",
 *     "192.168.1.50", 6000,    // destination IP:port
 *     "/data/crops",           // cropped images dir
 *     64, 64,                  // min crop size
 *     cvedix_broke_for::FACE,  // for face embeddings
 *     true                     // only tracked targets
 * );
 * broker->attach_to({face_recognition_node});
 * @endcode
 * 
 * @see cvedix_embeddings_properties_socket_broker_node For embeddings + properties
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

// light weight socket support
#include "cvedix/third_party/kissnet/kissnet.hpp"

namespace cvedix_nodes {

    /**
     * @brief UDP socket broker for embeddings only
     * 
     * Exports embedding vectors via UDP, saves cropped images.
     * Supports both cvedix_frame_target and cvedix_frame_face_target.
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_embeddings_socket_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Directory to save cropped target images
        std::string cropped_dir = "cropped_images";
        /// @brief Minimum target width for processing
        int min_crop_width = 50;
        /// @brief Minimum target height for processing
        int min_crop_height = 50;
        /// @brief Only process tracked targets (track_id != -1)
        bool only_for_tracked = false;

        /// @brief Minimum frames tracked before exporting
        int min_tracked_frames = 25;

        /// @brief Destination IP address
        std::string des_ip = "";
        /// @brief Destination port number
        int des_port = 0;

        /// @brief UDP socket writer
        kissnet::udp_socket udp_writer;

        /// @brief Tracks exported targets per channel
        std::map<int, std::vector<int>> all_broked;

    protected:
        /**
         * @brief Format embeddings to message
         * @param meta Frame meta with embeddings
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
         * 
         * @param node_name Unique node identifier
         * @param des_ip Destination IP address
         * @param des_port Destination UDP port
         * @param cropped_dir Directory for cropped images
         * @param min_crop_width Minimum target width
         * @param min_crop_height Minimum target height
         * @param broke_for Target type (NORMAL or FACE)
         * @param only_for_tracked Only export tracked targets
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         */
        cvedix_embeddings_socket_broker_node(std::string node_name, 
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
        ~cvedix_embeddings_socket_broker_node();
    };
}