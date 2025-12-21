/**
 * @file cvedix_expr_socket_broker_node.h
 * @brief UDP socket broker for math expression checking results
 * 
 * This node exports OCR-based math expression validation results via UDP.
 * Used for educational applications to check mathematical handwriting.
 * 
 * @section expr_overview Overview
 * After OCR detects text targets containing math expressions:
 * 1. Validates the mathematical expression
 * 2. Saves screenshot for web client display
 * 3. Sends validation result via UDP
 * 
 * @section expr_target Target Type
 * This broker only processes `cvedix_frame_text_target` objects.
 * Set `broke_for = cvedix_broke_for::TEXT` (default).
 * 
 * @section expr_usage Usage Example
 * @code
 * auto broker = std::make_shared<cvedix_expr_socket_broker_node>(
 *     "expr_broker",
 *     "192.168.1.50", 7000,     // destination IP:port
 *     "/data/screenshots",      // screenshot save directory
 *     cvedix_broke_for::TEXT
 * );
 * broker->attach_to({ocr_node});
 * @endcode
 * 
 * @see cvedix_msg_broker_node Base class
 * @see cvedix_frame_text_target OCR text detection result
 */

#pragma once

#include "cvedix_msg_broker_node.h"
#include "cvedix/objects/ba/cvedix_ba_result.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

// light weight socket support
#include "cvedix/third_party/kissnet/kissnet.hpp"

namespace cvedix_nodes {

    /**
     * @brief UDP socket broker for math expression validation results
     * 
     * Exports OCR text validation results for mathematical expressions.
     * Saves screenshots and sends validation data via UDP.
     * 
     * @note Only processes cvedix_frame_text_target (broke_for::TEXT)
     * 
     * @see cvedix_msg_broker_node Base class
     */
    class cvedix_expr_socket_broker_node: public cvedix_msg_broker_node
    {
    private:
        /// @brief Directory to save screenshot images (for web display)
        std::string screenshot_dir = "screenshot_images";
        /// @brief Destination IP address
        std::string des_ip = "";
        /// @brief Destination port number
        int des_port = 0;

        /// @brief UDP socket writer
        kissnet::udp_socket udp_writer;

    protected:
        /**
         * @brief Format expression validation result to message
         * @param meta Frame meta with text targets
         * @param[out] msg Output message string
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
         * @param screenshot_dir Directory for screenshots (default: "screenshot_images")
         * @param broke_for Target type, should be TEXT (default: TEXT)
         * @param broking_cache_warn_threshold Queue warning threshold
         * @param broking_cache_ignore_threshold Queue ignore threshold
         */
        cvedix_expr_socket_broker_node(std::string node_name, 
                                std::string des_ip = "",
                                int des_port = 0,
                                std::string screenshot_dir = "screenshot_images",
                                cvedix_broke_for broke_for = cvedix_broke_for::TEXT, 
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);

        /// @brief Destructor
        ~cvedix_expr_socket_broker_node();
    };
}