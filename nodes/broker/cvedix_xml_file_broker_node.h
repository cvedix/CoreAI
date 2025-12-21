/**
 * @file cvedix_xml_file_broker_node.h
 * @brief Debug broker that writes XML to file
 * 
 * Serializes frame_meta to XML format and writes to file.
 * For demo/debug purposes.
 * 
 * @section xml_file_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_xml_file_broker_node>(
 *     "xml_logger",
 *     cvedix_broke_for::NORMAL,
 *     "/tmp/detections.xml"
 * );
 * broker->attach_to({detector_node});
 * @endcode
 * 
 * @see cvedix_xml_socket_broker_node For UDP output
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#include <fstream>

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {

    /**
     * @brief XML file output broker
     * 
     * Writes detection results as XML to file.
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_xml_file_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief File output stream
        ofstream xml_writer;

    protected:
        /**
         * @brief Serialize to XML
         * @param meta Frame meta
         * @param[out] msg Output XML string
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief Write to file
         * @param msg XML message
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param broke_for Target type
         * @param file_path_and_name Output file path
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         */
        cvedix_xml_file_broker_node(std::string node_name, 
                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                std::string file_path_and_name = "",
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_xml_file_broker_node();
    };
}