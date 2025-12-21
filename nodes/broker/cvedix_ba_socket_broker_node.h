/**
 * @file cvedix_ba_socket_broker_node.h
 * @brief UDP socket broker node for behavior analysis results
 * 
 * This node publishes behavior analysis (BA) results via UDP socket.
 * Useful for real-time integration with external monitoring systems.
 * 
 * @section ba_socket_overview Overview
 * Serializes BA results (crossline, stop, jam events) to JSON format
 * and sends them to a specified UDP endpoint for external consumption.
 * 
 * @section ba_socket_protocol Protocol
 * - **Transport**: UDP (fire-and-forget, no acknowledgment)
 * - **Format**: JSON (via Cereal serialization)
 * - **Use case**: Real-time monitoring dashboards, alerting systems
 * 
 * @section ba_socket_usage Usage Example
 * @code
 * auto broker = std::make_shared<cvedix_ba_socket_broker_node>(
 *     "ba_socket",
 *     "192.168.1.100",    // destination IP
 *     5000,               // destination port
 *     cvedix_broke_for::NORMAL
 * );
 * broker->attach_to({crossline_node});
 * @endcode
 * 
 * @see cvedix_msg_broker_node Base class
 * @see cvedix_ba_crossline_node, cvedix_ba_jam_node, cvedix_ba_stop_node BA nodes
 */

#pragma once

#include "cvedix_msg_broker_node.h"
#include "cvedix/objects/ba/cvedix_ba_result.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

// light weight socket support
#include "cvedix/third_party/kissnet/kissnet.hpp"

namespace cvedix_nodes {

    /**
     * @brief UDP socket broker for behavior analysis results
     * 
     * Publishes BA event results (crossline, stop, jam) to external systems
     * via UDP socket in JSON format.
     * 
     * @note Currently supports cvedix_frame_target only (broke_for::NORMAL)
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_ba_socket_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Destination IP address for UDP packets
        std::string des_ip = "";
        /// @brief Destination port number
        int des_port = 0;

        /// @brief UDP socket writer (kissnet library)
        kissnet::udp_socket udp_writer;

    protected:
        /**
         * @brief Format BA results to JSON string
         * @param meta Frame meta containing BA results
         * @param[out] msg Output JSON message
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief Send message via UDP socket
         * @param msg JSON message to send
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Unique node identifier
         * @param des_ip Destination IP address (e.g., "192.168.1.100")
         * @param des_port Destination UDP port number
         * @param broke_for Target type (default: NORMAL)
         * @param broking_cache_warn_threshold Queue warning threshold (default: 50)
         * @param broking_cache_ignore_threshold Queue ignore threshold (default: 200)
         */
        cvedix_ba_socket_broker_node(std::string node_name, 
                                std::string des_ip = "",
                                int des_port = 0,
                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_ba_socket_broker_node();
    };
}