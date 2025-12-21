/**
 * @file cvedix_app_des_node.h
 * @brief Application destination node with callback interface
 * 
 * Sends processed meta data to host application via callbacks.
 * Use this for custom application integration.
 * 
 * @section app_des_usage Usage
 * @code
 * auto app_des = std::make_shared<cvedix_app_des_node>("app_output", 0);
 * app_des->set_app_des_result_hooker([](std::string name, auto meta) {
 *     // Process detection results in your app
 *     auto frame_meta = std::dynamic_pointer_cast<cvedix_frame_meta>(meta);
 *     if (frame_meta) {
 *         for (auto& target : frame_meta->targets) {
 *             // Handle each detection
 *         }
 *     }
 * });
 * app_des->attach_to({detector_node});
 * @endcode
 * 
 * @see cvedix_des_node Base class
 */

#pragma once

#include <iostream>
#include <memory>
#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "cvedix/nodes/common/cvedix_des_node.h"
#include "cvedix/objects/cvedix_frame_meta.h"
#include "cvedix/objects/cvedix_control_meta.h"
#include "cvedix/utils/cvedix_utils.h"


namespace cvedix_nodes {

    /**
     * @brief Callback type for receiving processed meta data
     * @param node_name Name of the source node
     * @param meta Processed meta data (frame or control)
     */
    typedef std::function<void(std::string, std::shared_ptr<cvedix_objects::cvedix_meta>)> cvedix_app_des_result_hooker;

    /**
     * @brief Application destination node with callback interface
     * 
     * Delivers detection results to host application via user-provided callback.
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_app_des_node: public cvedix_des_node {
    private:
        /// @brief User callback for receiving results
        cvedix_app_des_result_hooker app_des_result_hooker;

        /**
         * @brief Invoke user callback with meta data
         * @param meta Meta data to deliver
         */
        void invoke_app_des_result_hooker(std::shared_ptr<cvedix_objects::cvedix_meta> meta);

    protected:
        /**
         * @brief Process frame meta and invoke callback
         * @param meta Frame meta to process
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 

        /**
         * @brief Process control meta and invoke callback
         * @param meta Control meta to process
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param channel_index Channel index to process
         */
        cvedix_app_des_node(std::string node_name, 
                        int channel_index);

        /// @brief Destructor
        ~cvedix_app_des_node();

        /**
         * @brief Set callback for receiving results
         * @param app_des_result_hooker Callback function
         */
        void set_app_des_result_hooker(cvedix_app_des_result_hooker app_des_result_hooker);
    };
}