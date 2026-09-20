
#ifdef CVEDIX_WITH_GSTREAMER
#include "cvedix_rtsp_des_node.h"
#include "cvedix/nodes/common/frame_utils.h"
#include "cvedix/utils/cvedix_utils.h"
#include <thread>

namespace cvedix_nodes {
        
    cvedix_rtsp_des_node::cvedix_rtsp_des_node(std::string node_name, 
                        int channel_index, 
                        int rtsp_port, 
                        std::string rtsp_name, 
                        cvedix_objects::cvedix_size resolution_w_h, 
                        int bitrate,
                        bool osd,
                        std::string gst_encoder_name):
                        cvedix_des_node(node_name, channel_index),
                        rtsp_port(rtsp_port),
                        rtsp_name(rtsp_name),
                        resolution_w_h(resolution_w_h),
                        bitrate(bitrate),
                        osd(osd),
                        gst_encoder_name(gst_encoder_name) {
        if (rtsp_name.empty()) {
            // use channel index as rtsp name
            this->rtsp_name = std::to_string(channel_index);
        }
        gst_template = cvedix_utils::string_format(gst_template, gst_encoder_name.c_str(), bitrate, base_udp_port + channel_index);
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), gst_template.c_str()));

        // start rtsp server asynchronously
        start_rtsp_streaming();

        this->initialized();
    }
    
    cvedix_rtsp_des_node::~cvedix_rtsp_des_node() { 
        deinitialized();
    }
    GstRTSPServer* cvedix_rtsp_des_node::rtsp_server = NULL;
    GMainLoop* cvedix_rtsp_des_node::rtsp_main_loop = NULL;
    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_rtsp_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));
        
        auto resize_frame = utils::prepare_output_frame(meta, osd, resolution_w_h);

        // Open the encoder lazily on the first frame. The call must be a real
        // statement, not an assert(): in a Release build (NDEBUG) assert()
        // discards its argument, so the writer would never open and write()
        // would silently drop every frame.
        if (!rtsp_writer.isOpened()) {
            // Back off between attempts so a broken pipeline does not spawn a
            // GStreamer pipeline on every single frame.
            if (reconnect_cooldown_until > std::chrono::steady_clock::now())
                return cvedix_des_node::handle_frame_meta(meta);

            if (!rtsp_writer.open(this->gst_template, cv::CAP_GSTREAMER, 0, meta->fps,
                                  {resize_frame.cols, resize_frame.rows})) {
                reconnect_attempts++;
                reconnect_cooldown_until = std::chrono::steady_clock::now()
                                          + std::chrono::seconds(3);
                CVEDIX_WARN(cvedix_utils::string_format(
                    "[%s] cannot open RTSP writer (attempt #%d), retry in 3s: %s",
                    node_name.c_str(), reconnect_attempts, gst_template.c_str()));
                return cvedix_des_node::handle_frame_meta(meta);
            }

            reconnect_attempts = 0;
            CVEDIX_INFO(cvedix_utils::string_format(
                "[%s] RTSP writer opened: %dx%d @ %dfps -> %s",
                node_name.c_str(), resize_frame.cols, resize_frame.rows,
                meta->fps, to_string().c_str()));
        }

        rtsp_writer.write(resize_frame);

        // for general works defined in base class
        return cvedix_des_node::handle_frame_meta(meta);
    }

    void cvedix_rtsp_des_node::start_rtsp_streaming () {
        // create a rtsp server using gst-rtsp-server
        if (rtsp_server == NULL) {
            char port_num_Str[64] = { 0 };
            sprintf (port_num_Str, "%d", rtsp_port);
            rtsp_server = gst_rtsp_server_new ();
            g_object_set (rtsp_server, "service", port_num_Str, NULL);

            // gst-rtsp-server only answers DESCRIBE/SETUP/PLAY while a GMainLoop
            // iterates the context the server is attached to. Attaching to the
            // default context is not enough: a pipeline that has no GLib loop of
            // its own still accepts the TCP connection, then leaves the client
            // hanging forever. Give the server a private context and run its loop
            // on a dedicated thread.
            GMainContext* context = g_main_context_new ();
            gst_rtsp_server_attach (rtsp_server, context);
            rtsp_main_loop = g_main_loop_new (context, FALSE);
            std::thread([context] {
                g_main_context_push_thread_default (context);
                g_main_loop_run (cvedix_rtsp_des_node::rtsp_main_loop);
            }).detach();
        }

        char udpsrc_pipeline[512];
        if (udp_buffer_size == 0)
            udp_buffer_size = 512 * 1024;
        
        // receive stream data from udpsrc internally and push it via rtsp
        sprintf (udpsrc_pipeline,
                "( udpsrc name=pay0 port=%d buffer-size=%lu caps=\"application/x-rtp, media=video, "
                "clock-rate=90000, encoding-name=H264, payload=96 \" )",
                base_udp_port + channel_index, udp_buffer_size);
            
        auto mounts = gst_rtsp_server_get_mount_points (rtsp_server);

        auto factory = gst_rtsp_media_factory_new ();
        gst_rtsp_media_factory_set_launch (factory, udpsrc_pipeline);
        gst_rtsp_media_factory_set_shared (factory, TRUE);
        gst_rtsp_mount_points_add_factory (mounts, ("/" + rtsp_name).c_str(), factory);
        g_object_unref (mounts);

        CVEDIX_INFO(cvedix_utils::string_format("[%s] is going to push rtsp stream, please visit:[%s]", node_name.c_str(), to_string().c_str()));
    }

    std::string cvedix_rtsp_des_node::to_string() {
        return "rtsp://localhost:" + std::to_string(rtsp_port) + "/" + rtsp_name;
    }
}

#endif // CVEDIX_WITH_GSTREAMER