/**
 * @file cvedix_image_des_node.h
 * @brief Image capture destination node (file or UDP)
 * 
 * Captures periodic images to local files or streams via UDP.
 * 
 * @section image_des_modes Modes
 * - **File mode**: Location ends with `.jpg/.jpeg` (e.g., `/data/%d.jpg`)
 * - **UDP mode**: Location is `ip:port` (e.g., `192.168.1.50:8000`)
 * 
 * @section image_des_usage Usage
 * @code
 * // File mode - save image every 5 seconds
 * auto image_des = std::make_shared<cvedix_image_des_node>(
 *     "image_saver", 0,
 *     "/captures/%d.jpg",  // %d = sequence number
 *     5                    // 5 second interval
 * );
 * 
 * // UDP mode - stream images
 * auto udp_des = std::make_shared<cvedix_image_des_node>(
 *     "udp_streamer", 0,
 *     "192.168.1.50:8000", 1  // 1 fps
 * );
 * @endcode
 * 
 * @see cvedix_des_node Base class
 */

#pragma once

#include "cvedix/nodes/common/cvedix_des_node.h"

namespace cvedix_nodes {

    /**
     * @brief Image capture destination node
     * 
     * Periodic image capture to file or UDP stream.
     * 
     * @see cvedix_des_node Base class
     */
    class cvedix_image_des_node: public cvedix_des_node
    {
    private:
        /// @brief GStreamer template for file output
        std::string gst_template_file = "appsrc ! videoconvert ! videoscale ! videorate ! video/x-raw,framerate=1/%d ! %s ! multifilesink location=%s";
        /// @brief GStreamer template for UDP output
        std::string gst_template_udp = "appsrc ! videoconvert ! videoscale ! videorate ! video/x-raw,format=I420,framerate=1/%d ! %s ! rtpjpegpay ! udpsink host=%s port=%d";
        
        /// @brief OpenCV video writer
        cv::VideoWriter image_writer;

        /// @brief Capture interval (seconds)
        int interval;
        /// @brief Output location (file path or ip:port)
        std::string location = "./%d.jpg";

        /// @brief OSD enabled
        bool osd = true;
        /// @brief Output resolution
        cvedix_objects::cvedix_size resolution_w_h;

        /// @brief File mode (true) or UDP mode (false)
        bool to_file = true;
        /// @brief JPEG encoder name
        std::string gst_encoder_name = "jpegenc";

    protected:
        /**
         * @brief Capture and output image
         * @param meta Frame to capture
         * @return nullptr (terminal node)
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 

    public:
        /**
         * @brief Constructor
         * @param node_name Unique node identifier
         * @param channel_index Channel index
         * @param location File path (with %d) or ip:port
         * @param interval Capture interval (seconds)
         * @param resolution_w_h Output resolution
         * @param osd Enable OSD overlay
         * @param gst_encoder_name JPEG encoder (default: jpegenc)
         */
        cvedix_image_des_node(std::string node_name, 
                        int channel_index,  
                        std::string location = "./%d.jpg", 
                        int interval = 5,
                        cvedix_objects::cvedix_size resolution_w_h = {},
                        bool osd = true,
                        std::string gst_encoder_name = "jpegenc");

        /// @brief Destructor
        ~cvedix_image_des_node();

        /// @brief Get node description
        virtual std::string to_string() override;
    };

}