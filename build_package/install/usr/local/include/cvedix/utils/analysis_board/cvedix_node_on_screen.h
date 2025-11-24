
#pragma once

#include <memory>
#include <opencv2/core.hpp>

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/nodes/common/cvedix_src_node.h"
#include "cvedix/nodes/common/cvedix_des_node.h"
#include "cvedix/objects/shapes/cvedix_rect.h"
#include "cvedix/utils/cvedix_utils.h"

namespace cvedix_utils {

    // mainly used to store data from meta hookers' callback
    struct cvedix_meta_hooker_storage {
        int queue_size = -1;                      // size of in/out queue of node
        int latency = 0;                          // latency(ms) relative to src node at current port
        int called_count_since_epoch_start = -1;  // used for calculating fps at current port 
        std::chrono::system_clock::time_point time_epoch_start;      // used for calculating fps at current port
        std::chrono::system_clock::time_point last_active_time;      // used for flashing effect
        std::shared_ptr<cvedix_objects::cvedix_meta> meta = nullptr;         // the latest meta (ptr) flowing through current port inside node (total 4 ports) 
        std::string pre_fps;  // cache
    };

    // a class corresponding to cvedix_node, used to display node on screen and map the whole pipe from memery to screen.
    class cvedix_node_on_screen
    {
    private:
        // orignal node in memery
        std::shared_ptr<cvedix_nodes::cvedix_node> original_node = nullptr;
        // node location and size on screen
        cvedix_objects::cvedix_rect node_rect;
        // layer index in pipe
        int layer;
        // nodes in next layer on screen
        std::vector<std::shared_ptr<cvedix_node_on_screen>> next_nodes_on_screen; 

        // period to calculate fps, milliseconds
        int fps_epoch = 500;

        // reset fps if it has been long time no update, seconds
        int fps_timeout = 5;

        // font for display
        int font_face = cv::FONT_HERSHEY_SIMPLEX;

        // container to store data from meta hookers' callbacks
        cvedix_meta_hooker_storage meta_arriving_hooker_storage;
        cvedix_meta_hooker_storage meta_handling_hooker_storage;
        cvedix_meta_hooker_storage meta_handled_hooker_storage;
        cvedix_meta_hooker_storage meta_leaving_hooker_storage;

        // container to store data from stream info hooker's callback
        cvedix_nodes::cvedix_stream_info stream_info_hooker_storage;

        // container to store data from stream status hooker's callback
        cvedix_nodes::cvedix_stream_status stream_status_hooker_storage;

        // render configure
        const int node_title_h = 24;
        const int node_queue_width = 30;

        const int node_queue_port_w_h = 6;
        const int node_queue_port_padding = 8;
        const int node_gap_horizontal = 40;
        const int node_gap_vertical = 10;

        // Brand text configuration
        const std::string brand_text = "CVEDIX Instance Pipeline";
        const int brand_text_padding = 5;  // Padding from left edge
    public:
        cvedix_node_on_screen(std::shared_ptr<cvedix_nodes::cvedix_node> original_node, cvedix_objects::cvedix_rect node_rect, int layer);
        ~cvedix_node_on_screen();

        // render static parts for node, which keep unchanged all the time.
        void render_static_parts(cv::Mat& canvas);
        // render dynamic parts for node, which change frequently.
        void render_dynamic_parts(cv::Mat& canvas);

        std::shared_ptr<cvedix_nodes::cvedix_node>& get_orginal_node();

        std::vector<std::shared_ptr<cvedix_node_on_screen>>& get_next_nodes_on_screen();
    };
}