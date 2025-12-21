/**
 * @file cvedix_xml_socket_broker_node.h
 * @brief UDP socket broker for XML output
 * 
 * Serializes frame_meta to XML format and sends via UDP.
 * 
 * @section xml_socket_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_xml_socket_broker_node>(
 *     "xml_udp",
 *     "192.168.1.50", 9000
 * );
 * broker->attach_to({detector_node});
 * @endcode
 * 
 * @see cvedix_xml_file_broker_node For file output
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

// light weight socket support
#include "cvedix/third_party/kissnet/kissnet.hpp"

namespace cvedix_nodes {

    /**
     * @brief UDP XML broker
     * 
     * Sends detection results as XML via UDP.
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_xml_socket_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Destination IP address
        std::string des_ip = "";
        /// @brief Destination port
        int des_port = 0;

        /// @brief UDP socket writer
        kissnet::udp_socket udp_writer;

    protected:
        /**
         * @brief Serialize to XML
         * @param meta Frame meta
         * @param[out] msg Output XML string
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief Send via UDP socket
         * @param msg XML message
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param des_ip Destination IP
         * @param des_port Destination port
         * @param broke_for Target type
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         */
        cvedix_xml_socket_broker_node(std::string node_name, 
                                std::string des_ip = "",
                                int des_port = 0,
                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_xml_socket_broker_node();
    };
}