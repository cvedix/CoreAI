/**
 * @file cvedix_json_console_broker_node.h
 * @brief Debug broker that outputs JSON to console
 * 
 * Simple broker for debugging - serializes frame_meta to JSON and prints to stdout.
 * 
 * @section json_console_usage Usage
 * @code
 * auto broker = std::make_shared<cvedix_json_console_broker_node>("debug");
 * broker->attach_to({detector_node});
 * // Output appears in terminal as JSON
 * @endcode
 * 
 * @see cvedix_json_enhanced_console_broker_node For base64 image support
 * @see cvedix_msg_broker_node Base class
 */

#pragma once

#include <sstream>

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {

    /**
     * @brief Debug broker - JSON output to console
     * 
     * For debugging pipelines. Prints detection results as JSON to stdout.
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_json_console_broker_node: public cvedix_msg_broker_node
    {
    private:
        /* data */

    protected:
        /**
         * @brief Serialize frame_meta to JSON
         * @param meta Frame meta to serialize
         * @param[out] msg Output JSON string
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;

        /**
         * @brief Print JSON to console (stdout)
         * @param msg JSON message to print
         */
        virtual void broke_msg(const std::string& msg) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param broke_for Target type to serialize
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         */
        cvedix_json_console_broker_node(std::string node_name, 
                                    cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                    int broking_cache_warn_threshold = 50, 
                                    int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_json_console_broker_node();
    };
}