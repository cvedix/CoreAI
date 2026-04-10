/**
 * @file cvedix_msg_broker_node.h
 * @brief Base class for message broker nodes in Core AI Runtime
 * 
 * This file defines the cvedix_msg_broker_node base class for serializing
 * pipeline data and pushing it to external systems (Kafka, MQTT, files, sockets, etc.).
 * 
 * @section broker_overview Overview
 * Message broker nodes are used to export detection/analysis results from the
 * pipeline to external consumers. They work asynchronously to avoid blocking
 * the main processing pipeline.
 * 
 * @section broker_types Broker Types by Target
 * - **NORMAL**: Works with cvedix_frame_target (generic object detection)
 * - **FACE**: Works with cvedix_frame_face_target (face detection/recognition)
 * - **TEXT**: Works with cvedix_frame_text_target (OCR/text detection)
 * - **POSE**: Works with cvedix_frame_pose_target (pose estimation)
 * 
 * @section broker_usage Usage Example
 * @code
 * // Create JSON MQTT broker
 * auto broker = std::make_shared<cvedix_mqtt_broker_node>(
 *     "mqtt_broker",
 *     cvedix_broke_for::FACE,  // For face recognition results
 *     50,   // warn threshold
 *     200,  // ignore threshold
 *     nullptr,  // json transformer
 *     mqtt_publish_func  // your MQTT publish function
 * );
 * 
 * // Attach to pipeline
 * broker->attach_to({face_recognition_node});
 * @endcode
 * 
 * @see cvedix_mqtt_broker_node For MQTT publishing
 * @see cvedix_kafka_broker_node For Kafka publishing
 * @see cvedix_console_broker_node For console output
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {

    /**
     * @brief Specifies the data type that the broker node will serialize
     * 
     * Different target types contain different data structures and require
     * different serialization logic.
     */
    enum class cvedix_broke_for {
        NORMAL,  ///< cvedix_frame_target - generic object detection results
        FACE,    ///< cvedix_frame_face_target - face detection/recognition results
        TEXT,    ///< cvedix_frame_text_target - text/OCR detection results
        POSE     ///< cvedix_frame_pose_target - body pose estimation results
    };

    /**
     * @brief Base class for all message broker nodes
     * 
     * Message brokers serialize frame meta objects to structured data (JSON, XML, etc.)
     * and push them to external systems for further processing or storage.
     * 
     * @section broker_features Key Features
     * - **Asynchronous**: Uses separate thread to avoid blocking pipeline
     * - **Buffered**: Maintains queue with configurable thresholds
     * - **Extensible**: Derived classes implement format_msg() and broke_msg()
     * 
     * @section broker_thresholds Cache Thresholds
     * - **Warn threshold** (default 50): Log warning when queue exceeds this size
     * - **Ignore threshold** (default 200): Drop new messages when queue exceeds
     * 
     * @note This is an abstract base class - cannot be instantiated directly.
     *       Use concrete implementations like cvedix_mqtt_broker_node.
     * 
     * @see cvedix_node Base class
     */
    class cvedix_msg_broker_node: public cvedix_node
    {
    private:
        /// @brief Warning threshold for cache size (default: 50)
        int broking_cache_warn_threshold = 50;
        /// @brief Flag indicating if warning has been logged
        bool broking_cache_warned = false;

        /// @brief Ignore threshold - skip messages when exceeded (default: 200)
        int broking_cache_ignore_threshold = 200;

        /// @brief Queue of frames pending serialization and publishing
        std::queue<std::shared_ptr<cvedix_objects::cvedix_frame_meta>> frames_to_broke;
        /// @brief Semaphore for thread synchronization
        cvedix_utils::cvedix_semaphore broking_cache_semaphore;

        /// @brief Background thread for async message publishing
        std::thread broking_th;
        /// @brief Main loop function for the broking thread
        void broking_run();
        /// @brief Flag to control broking thread lifecycle
        bool broking = true;

    protected:
        /**
         * @brief Handle incoming frame meta - queues for async processing
         * @param meta Frame meta to queue
         * @return The same meta passed through to next nodes
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override final;

        /**
         * @brief Handle control meta - pass through without processing
         * @param meta Control meta
         * @return The same meta passed through
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override final;

        /**
         * @brief Serialize frame meta to message string
         * 
         * Pure virtual method - must be implemented by derived classes to define
         * the serialization format (JSON, XML, binary, etc.).
         * 
         * @param meta Frame meta to serialize
         * @param[out] msg Output message string
         */
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) = 0;

        /**
         * @brief Publish message to external system
         * 
         * Pure virtual method - must be implemented by derived classes to define
         * the transport mechanism (Kafka, MQTT, file, socket, etc.).
         * 
         * @param msg Serialized message to publish
         */
        virtual void broke_msg(const std::string& msg) = 0;

        /**
         * @brief Stop the background broking thread
         * 
         * Should be called by derived class destructors before destroying resources.
         */
        void stop_broking();

        /// @brief The target type this broker is configured for
        cvedix_broke_for broke_for = cvedix_broke_for::NORMAL;

        /// @brief String representations of broke_for values
        std::map<cvedix_broke_for, std::string> broke_fors = {
            {cvedix_broke_for::NORMAL, "normal"}, 
            {cvedix_broke_for::FACE, "face"}, 
            {cvedix_broke_for::TEXT, "text"}, 
            {cvedix_broke_for::POSE, "pose"}
        };

    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Unique identifier for this node
         * @param broke_for Target data type to serialize (default: NORMAL)
         * @param broking_cache_warn_threshold Queue size for warning (default: 50)
         * @param broking_cache_ignore_threshold Queue size to start dropping (default: 200)
         */
        cvedix_msg_broker_node(std::string node_name, 
                        cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                        int broking_cache_warn_threshold = 50, 
                        int broking_cache_ignore_threshold = 200);

        /// @brief Destructor - stops broking thread
        ~cvedix_msg_broker_node();
    };
}