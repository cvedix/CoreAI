
#include "cvedix_node_on_screen.h"


namespace cvedix_utils {
        
    cvedix_node_on_screen::cvedix_node_on_screen(std::shared_ptr<cvedix_nodes::cvedix_node> original_node, 
                                        cvedix_objects::cvedix_rect node_rect, 
                                        int layer):
                                        original_node(original_node),
                                        node_rect(node_rect),
                                        layer(layer) {
        assert(original_node != nullptr);
        // register meta hookers for all nodes
        original_node->set_meta_arriving_hooker([this](std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
                std::lock_guard<std::mutex> guard(this->hooker_mutex);
                this->meta_arriving_hooker_storage.meta = meta;
                this->meta_arriving_hooker_storage.queue_size = queue_size;
                this->meta_arriving_hooker_storage.called_count_since_epoch_start++;
                this->meta_arriving_hooker_storage.last_active_time = std::chrono::system_clock::now();
            });
        original_node->set_meta_handling_hooker([this](std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
                std::lock_guard<std::mutex> guard(this->hooker_mutex);
                this->meta_handling_hooker_storage.meta = meta;
                this->meta_handling_hooker_storage.queue_size = queue_size;
                this->meta_handling_hooker_storage.called_count_since_epoch_start++;
                this->meta_handling_hooker_storage.last_active_time = std::chrono::system_clock::now();
            });
        original_node->set_meta_handled_hooker([this](std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
                std::lock_guard<std::mutex> guard(this->hooker_mutex);
                this->meta_handled_hooker_storage.meta = meta;
                this->meta_handled_hooker_storage.queue_size = queue_size;
                this->meta_handled_hooker_storage.called_count_since_epoch_start++;
                this->meta_handled_hooker_storage.last_active_time = std::chrono::system_clock::now();
            });
        original_node->set_meta_leaving_hooker([this](std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
                std::lock_guard<std::mutex> guard(this->hooker_mutex);
                this->meta_leaving_hooker_storage.meta = meta;
                this->meta_leaving_hooker_storage.queue_size = queue_size;
                this->meta_leaving_hooker_storage.called_count_since_epoch_start++;
                this->meta_leaving_hooker_storage.last_active_time = std::chrono::system_clock::now();
            });
        
        // register stream info hooker if it is a src node
        if (original_node->node_type() == cvedix_nodes::cvedix_node_type::SRC) {
            auto src_node = std::dynamic_pointer_cast<cvedix_nodes::cvedix_src_node>(original_node);
            src_node->set_stream_info_hooker([this](std::string node_name, cvedix_nodes::cvedix_stream_info stream_info) {
                this->stream_info_hooker_storage = stream_info;
            });
        }
        if (original_node->node_type() == cvedix_nodes::cvedix_node_type::DES) {
            auto des_node = std::dynamic_pointer_cast<cvedix_nodes::cvedix_des_node>(original_node);
            des_node->set_stream_status_hooker([this](std::string node_name, cvedix_nodes::cvedix_stream_status stream_status){
                this->stream_status_hooker_storage = stream_status;
            });
        }
        
    }
    
    cvedix_node_on_screen::~cvedix_node_on_screen() {
        // unregister meta hookers for all nodes
        original_node->set_meta_arriving_hooker({});
        original_node->set_meta_handling_hooker({});
        original_node->set_meta_handled_hooker({});
        original_node->set_meta_leaving_hooker({});
        
        // unregister stream info hooker if it is a src node
        if (original_node->node_type() == cvedix_nodes::cvedix_node_type::SRC) {
            auto src_node = std::dynamic_pointer_cast<cvedix_nodes::cvedix_src_node>(original_node);
            src_node->set_stream_info_hooker({});
        }
        if (original_node->node_type() == cvedix_nodes::cvedix_node_type::DES) {
            auto des_node = std::dynamic_pointer_cast<cvedix_nodes::cvedix_des_node>(original_node);
            des_node->set_stream_status_hooker({});
        }
    }
    
    void cvedix_node_on_screen::render_static_parts(cv::Mat & canvas) {
        auto node_left = node_rect.x;
        auto node_top = node_rect.y;
        auto node_width = node_rect.width;
        auto node_height = node_rect.height;
        
        // Determine node type colors
        cv::Scalar node_bg_color, node_border_color, header_bg_color, header_text_color;
        int corner_radius = 8;
        
        if (original_node->node_type() == cvedix_nodes::cvedix_node_type::SRC) {
            // Source nodes: Blue gradient theme
            node_bg_color = cv::Scalar(240, 248, 255);  // Alice Blue background
            node_border_color = cv::Scalar(70, 130, 180);  // Steel Blue border
            header_bg_color = cv::Scalar(65, 105, 225);  // Royal Blue header
            header_text_color = cv::Scalar(255, 255, 255);  // White text
        } else if (original_node->node_type() == cvedix_nodes::cvedix_node_type::DES) {
            // Destination nodes: Green gradient theme
            node_bg_color = cv::Scalar(240, 255, 240);  // Honeydew background
            node_border_color = cv::Scalar(60, 179, 113);  // Medium Sea Green border
            header_bg_color = cv::Scalar(46, 139, 87);  // Sea Green header
            header_text_color = cv::Scalar(255, 255, 255);  // White text
        } else {
            // Middle nodes: Purple gradient theme
            node_bg_color = cv::Scalar(248, 248, 255);  // Ghost White background
            node_border_color = cv::Scalar(138, 43, 226);  // Blue Violet border
            header_bg_color = cv::Scalar(123, 104, 238);  // Medium Slate Blue header
            header_text_color = cv::Scalar(255, 255, 255);  // White text
        }
        
        // Draw shadow (offset by 2 pixels)
        cv::Rect shadow_rect(node_left + 2, node_top + 2, node_width, node_height);
        cvedix_utils::draw_rounded_rectangle(canvas, 
            cv::Point(shadow_rect.x, shadow_rect.y), 
            cv::Point(shadow_rect.x + shadow_rect.width, shadow_rect.y + shadow_rect.height),
            corner_radius, cv::Scalar(0, 0, 0, 0), -1, cv::Scalar(200, 200, 200), cv::LINE_AA);
        
        // Draw main node background with rounded corners
        cvedix_utils::draw_rounded_rectangle(canvas, 
            cv::Point(node_left, node_top), 
            cv::Point(node_left + node_width, node_top + node_height),
            corner_radius, node_border_color, 2, node_bg_color, cv::LINE_AA);
        
        // Draw header background with rounded top corners
        cv::Rect header_rect(node_left, node_top, node_width, node_title_h);
        cvedix_utils::draw_rounded_rectangle(canvas,
            cv::Point(header_rect.x, header_rect.y),
            cv::Point(header_rect.x + header_rect.width, header_rect.y + header_rect.height + corner_radius),
            corner_radius, header_bg_color, -1, header_bg_color, cv::LINE_AA);
        
        // Draw brand text "CVEDIX Instance Pipeline" at left side of header
        int brand_text_x = node_left + brand_text_padding;
        int brand_text_size = 0.5;  // Font scale for brand text
        int baseline = 0;
        cv::Size brand_text_sz = cv::getTextSize(brand_text, font_face, brand_text_size, 1, &baseline);
        int brand_text_y = node_top + (node_title_h + brand_text_sz.height) / 2;
        
        // Draw brand text with shadow effect for better visibility
        cv::putText(canvas, brand_text, 
                    cv::Point(brand_text_x + 1, brand_text_y + 1), 
                    font_face, brand_text_size, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
        cv::putText(canvas, brand_text, 
                    cv::Point(brand_text_x, brand_text_y), 
                    font_face, brand_text_size, header_text_color, 1, cv::LINE_AA);
        
        // Calculate remaining space for node name after brand text
        int brand_text_width = brand_text_sz.width + brand_text_padding * 2;
        int node_name_x = brand_text_x + brand_text_width + brand_text_padding;
        int node_name_width = node_width - node_name_x - brand_text_padding;
        
        // Draw node name to the right of brand text
        if (node_name_width > 20) {  // Only draw if there's enough space
            cvedix_utils::put_text_at_center_of_rect(canvas, original_node->node_name, 
                cv::Rect(node_name_x, node_top + 1, node_name_width, node_title_h - 2), 
                false, font_face, 1, header_text_color);
        } else {
            // If no space, center the node name (brand text will overlap slightly)
            cvedix_utils::put_text_at_center_of_rect(canvas, original_node->node_name, 
                cv::Rect(node_left, node_top + 1, node_width, node_title_h - 2), 
                false, font_face, 1, header_text_color);
        }
        
        // Draw separator line with gradient effect
        cv::line(canvas, 
                cv::Point(node_left + corner_radius, node_top + node_title_h), 
                cv::Point(node_left + node_width - corner_radius, node_top + node_title_h), 
                cv::Scalar(220, 220, 220), 1, cv::LINE_AA);

        // draw in_queue for non-src nodes
        if (original_node->node_type() != cvedix_nodes::cvedix_node_type::SRC) {
            // Improved internal connection lines
            cv::Scalar internal_connection_color(150, 150, 200);  // Light blue-gray for internal connections
            // connect line between in_queue and out_queue
            if (original_node->node_type() == cvedix_nodes::cvedix_node_type::MID) {
                cv::line(canvas, 
                        cv::Point(node_left + node_queue_width + node_queue_port_w_h, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_width / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2),
                        internal_connection_color, 1, cv::LINE_AA);
                cv::line(canvas, 
                        cv::Point(node_left + node_width / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_width / 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2),
                        internal_connection_color, 1, cv::LINE_AA);
                cv::line(canvas, 
                        cv::Point(node_left + node_width / 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2),
                        internal_connection_color, 1, cv::LINE_AA);
                std::vector<cv::Point> vertexs {cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                                                cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h * 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h), 
                                                cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h * 2, node_top + node_height - node_queue_port_padding)};
                cv::fillPoly(canvas, std::vector<std::vector<cv::Point>>{vertexs}, internal_connection_color);
            }
            else {
                // DES
                cv::line(canvas, 
                        cv::Point(node_left + node_queue_width + node_queue_port_w_h, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_width / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2),
                        internal_connection_color, 1, cv::LINE_AA);
                std::vector<cv::Point> vertexs {cv::Point(node_left + node_width / 2, node_top + node_title_h + node_queue_port_padding), 
                                                cv::Point(node_left + node_width / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h), 
                                                cv::Point(node_left + node_width / 2 + node_queue_port_w_h, node_top+ node_title_h + node_queue_port_padding + node_queue_port_w_h / 2)};
                cv::fillPoly(canvas, std::vector<std::vector<cv::Point>>{vertexs}, internal_connection_color);
            }

            // Queue separator line with softer color
            cv::line(canvas, 
                    cv::Point(node_left + node_queue_width, node_top + node_title_h), 
                    cv::Point(node_left + node_queue_width, node_top + node_height - 1), 
                    cv::Scalar(180, 180, 180), 1, cv::LINE_AA);

            // in port with better styling
            cv::circle(canvas, 
                        cv::Point(node_left - node_queue_port_w_h / 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(100, 149, 237), -1);  // Cornflower Blue filled
            cv::circle(canvas, 
                        cv::Point(node_left - node_queue_port_w_h / 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(70, 130, 180), 2);  // Steel Blue border

            // out port with better styling
            cv::circle(canvas, 
                        cv::Point(node_left + node_queue_width + node_queue_port_w_h / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(100, 149, 237), -1);  // Cornflower Blue filled
            cv::circle(canvas, 
                        cv::Point(node_left + node_queue_width + node_queue_port_w_h / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(70, 130, 180), 2);  // Steel Blue border
        }
        // draw out_queue for non-des nodes
        if (original_node->node_type() != cvedix_nodes::cvedix_node_type::DES) {
            // Improved internal connection lines
            cv::Scalar internal_connection_color(150, 150, 200);  // Light blue-gray for internal connections
            // connect line between in_queue and out_queue 
            if (original_node->node_type() == cvedix_nodes::cvedix_node_type::SRC) {
                cv::line(canvas, 
                        cv::Point(node_left + node_width / 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        internal_connection_color, 1, cv::LINE_AA);
                
                std::vector<cv::Point> vertexs {cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                                                cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h * 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h), 
                                                cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h * 2, node_top + node_height - node_queue_port_padding)};
                cv::fillPoly(canvas, std::vector<std::vector<cv::Point>>{vertexs}, internal_connection_color);
            }
            
            // Queue separator line with softer color
            cv::line(canvas, 
                    cv::Point(node_left + node_width - node_queue_width, node_top + node_title_h), 
                    cv::Point(node_left + node_width - node_queue_width, node_top + node_height - 1), 
                    cv::Scalar(180, 180, 180), 1, cv::LINE_AA);
            
            // in port with better styling
            cv::circle(canvas, 
                        cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h / 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(100, 149, 237), -1);  // Cornflower Blue filled
            cv::circle(canvas, 
                        cv::Point(node_left + node_width - node_queue_width - node_queue_port_w_h / 2, node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(70, 130, 180), 2);  // Steel Blue border
            // out port with better styling
            cv::circle(canvas,
                        cv::Point(node_left + node_width - node_queue_port_w_h / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(100, 149, 237), -1);  // Cornflower Blue filled
            cv::circle(canvas,
                        cv::Point(node_left + node_width - node_queue_port_w_h / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        node_queue_port_w_h / 2 + 1, 
                        cv::Scalar(70, 130, 180), 2);  // Steel Blue border
        }

        // draw blocks connect line between nodes and nodes
        auto draw_connect_block = [=](int next_node_top){
            // Improved connection lines with gradient color
            cv::Scalar connection_color(100, 149, 237);  // Cornflower Blue
            cv::line(canvas, 
                        cv::Point(node_left + node_width + node_queue_port_w_h, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_width + node_gap_horizontal / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2),
                        connection_color, 2, cv::LINE_AA);
            cv::line(canvas, 
                    cv::Point(node_left + node_width + node_gap_horizontal / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                    cv::Point(node_left + node_width + node_gap_horizontal / 2, next_node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2),
                    connection_color, 2, cv::LINE_AA);
            cv::line(canvas, 
                    cv::Point(node_left + node_width + node_gap_horizontal / 2, next_node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                    cv::Point(node_left + node_width + node_gap_horizontal - node_queue_port_w_h, next_node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2),
                    connection_color, 2, cv::LINE_AA);

            // Improved arrow with better color
            std::vector<cv::Point> vertexs {cv::Point(node_left + node_width + node_gap_horizontal - node_queue_port_w_h, next_node_top + node_height - node_queue_port_padding - node_queue_port_w_h / 2), 
                                            cv::Point(node_left + node_width + node_gap_horizontal - node_queue_port_w_h * 2, next_node_top + node_height - node_queue_port_padding - node_queue_port_w_h), 
                                            cv::Point(node_left + node_width + node_gap_horizontal - node_queue_port_w_h * 2, next_node_top + node_height - node_queue_port_padding)};
            cv::fillPoly(canvas, std::vector<std::vector<cv::Point>>{vertexs}, connection_color);};
        
        auto next_nodes_num = next_nodes_on_screen.size();
        for (int j = 0; j < next_nodes_num; j++) {
            draw_connect_block(next_nodes_on_screen[j]->node_rect.y);
        }     
    }

    void cvedix_node_on_screen::render_dynamic_parts(cv::Mat & canvas) {
        std::lock_guard<std::mutex> guard(hooker_mutex);
        /*
        * draw data from hookers' callbacks
        */
        auto fps_func = [&](cvedix_meta_hooker_storage& storage, cv::Rect rect) {
            auto called_count = storage.called_count_since_epoch_start;
            auto epoch_start = storage.time_epoch_start;
            auto delta_sec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - epoch_start);
            if (delta_sec.count() > fps_timeout * 1000 || (delta_sec.count() > fps_epoch && called_count > 0)) {
                //int fps = round(called_count * 1000.0 / delta_sec.count());
                auto fps = cvedix_utils::round_any(called_count * 1000.0 / delta_sec.count(), 1);
                storage.called_count_since_epoch_start = 0;
                storage.time_epoch_start = std::chrono::system_clock::now();
                storage.pre_fps = fps;  // cache for next show
                // FPS with better styling - blue theme
                cv::Scalar fps_bg_color(173, 216, 230);  // Light Blue background
                cv::Scalar fps_text_color(25, 25, 112);  // Midnight Blue text
                cvedix_utils::put_text_at_center_of_rect(canvas, 
                    fps, 
                    rect, true,
                    font_face, 1, fps_text_color, cv::Scalar(100, 150, 200), fps_bg_color);
            }
            else {
                // use previous fps with better styling
                cv::Scalar fps_bg_color(173, 216, 230);  // Light Blue background
                cv::Scalar fps_text_color(25, 25, 112);  // Midnight Blue text
                cvedix_utils::put_text_at_center_of_rect(canvas, 
                    storage.pre_fps, 
                    rect, true,
                    font_face, 1, fps_text_color, cv::Scalar(100, 150, 200), fps_bg_color);
            }
        };

        // non-src nodes
        if (original_node->node_type() != cvedix_nodes::cvedix_node_type::SRC) { 
            // size of in queue with better styling
            cv::Scalar queue_bg_color(255, 182, 193);  // Light Pink background
            cv::Scalar queue_text_color(139, 0, 139);  // Dark Magenta text
            cvedix_utils::put_text_at_center_of_rect(canvas, 
                                    std::to_string(std::max(meta_handling_hooker_storage.queue_size, 0)), 
                                    cv::Rect(node_rect.x + 3, 
                                    node_rect.y + node_title_h / 2 + (node_rect.height - node_title_h) / 2, node_queue_width  - 8, node_title_h - 10), 
                                    true, font_face, 1, queue_text_color, cv::Scalar(200, 100, 120), queue_bg_color);
            // fps at 1st port
            fps_func(meta_arriving_hooker_storage, cv::Rect(node_rect.x - node_queue_width / 2, 
                        node_rect.y + node_rect.height - node_queue_port_padding * 3 - node_queue_port_w_h * 3 / 2, 
                        node_queue_width, 
                        node_queue_port_padding + node_queue_port_w_h));
            // fps at 2nd port
            fps_func(meta_handling_hooker_storage, cv::Rect(node_rect.x + node_queue_width - node_queue_width / 2, 
                        node_rect.y + node_title_h + node_queue_port_padding * 3 / 2 + node_queue_port_w_h, 
                        node_queue_width, 
                        node_queue_port_padding + node_queue_port_w_h));
        }   
        
        // non-des nodes
        if (original_node->node_type() != cvedix_nodes::cvedix_node_type::DES) { 
            // size of out queue with better styling
            cv::Scalar queue_bg_color(255, 182, 193);  // Light Pink background
            cv::Scalar queue_text_color(139, 0, 139);  // Dark Magenta text
            cvedix_utils::put_text_at_center_of_rect(canvas, 
                                    std::to_string(std::max(meta_leaving_hooker_storage.queue_size, 0)), 
                                    cv::Rect(node_rect.x + node_rect.width - node_queue_width + 3, 
                                    node_rect.y + node_title_h / 2 + (node_rect.height - node_title_h) / 2, node_queue_width - 8, node_title_h - 10), 
                                    true, font_face, 1, queue_text_color, cv::Scalar(200, 100, 120), queue_bg_color);
            // fps at 3rd port
            fps_func(meta_handled_hooker_storage, cv::Rect(node_rect.x + node_rect.width - node_queue_width - node_queue_width / 2, 
                        node_rect.y + node_rect.height - node_queue_port_padding * 3 - node_queue_port_w_h * 3 / 2, 
                        node_queue_width, 
                        node_queue_port_padding + node_queue_port_w_h));
            // fps at 4th port
            fps_func(meta_leaving_hooker_storage, cv::Rect(node_rect.x + node_rect.width - node_queue_width / 2, 
                        node_rect.y + node_title_h + node_queue_port_padding * 3 / 2 + node_queue_port_w_h, 
                        node_queue_width, 
                        node_queue_port_padding  + node_queue_port_w_h));
        }  

        auto node_left = node_rect.x;
        auto node_top = node_rect.y;
        // stream info at src nodes
        if (original_node->node_type() == cvedix_nodes::cvedix_node_type::SRC) {
            // Stream info with better styling - light blue theme
            cv::Scalar info_bg_color(230, 240, 255);  // Very light blue background
            cv::Scalar info_text_color(25, 25, 112);  // Midnight Blue text
            cv::Scalar info_border_color(100, 149, 237);  // Cornflower Blue border
            
            cvedix_utils::put_text_at_center_of_rect(canvas, stream_info_hooker_storage.uri, 
                                                cv::Rect(node_left - node_rect.width * 3 / 4, node_top + node_title_h + node_queue_port_padding, node_rect.width * 4 / 3, node_title_h * 2 / 3), 
                                                true, font_face, 1, info_text_color, info_border_color, info_bg_color);
            cvedix_utils::put_text_at_center_of_rect(canvas, "original_width: " + std::to_string(stream_info_hooker_storage.original_width),
                                                cv::Rect(node_left - node_rect.width * 3 / 4, node_top + node_title_h * 5 / 3 + node_queue_port_padding * 2, node_rect.width * 4 / 3, node_title_h * 2 / 3),
                                                true, font_face, 1, info_text_color, info_border_color, info_bg_color);
            cvedix_utils::put_text_at_center_of_rect(canvas, "original_height: " + std::to_string(stream_info_hooker_storage.original_height),
                                                cv::Rect(node_left - node_rect.width * 3 / 4, node_top + node_title_h * 7 / 3 + node_queue_port_padding * 3, node_rect.width * 4 / 3, node_title_h * 2 / 3),
                                                true, font_face, 1, info_text_color, info_border_color, info_bg_color);      
            cvedix_utils::put_text_at_center_of_rect(canvas, "original_fps: " + std::to_string(stream_info_hooker_storage.original_fps),
                                                cv::Rect(node_left - node_rect.width * 3 / 4, node_top + node_title_h * 9 / 3 + node_queue_port_padding * 4, node_rect.width * 4 / 3, node_title_h * 2 / 3),
                                                true, font_face, 1, info_text_color, info_border_color, info_bg_color);   
        } 
        // stream status at des nodes
        if (original_node->node_type() == cvedix_nodes::cvedix_node_type::DES) {
            // Stream status with better styling - light green theme
            cv::Scalar status_bg_color(240, 255, 240);  // Honeydew background
            cv::Scalar status_text_color(0, 100, 0);  // Dark Green text
            cv::Scalar status_border_color(60, 179, 113);  // Medium Sea Green border
            
            cvedix_utils::put_text_at_center_of_rect(canvas, stream_status_hooker_storage.direction,
                                                cv::Rect(node_left + node_rect.width / 2 - 10, node_top + node_title_h * 5 / 3 + node_queue_port_padding * 2, node_rect.width * 4 / 3 + 10, node_title_h * 2 / 3),
                                                true, font_face, 1, status_text_color, status_border_color, status_bg_color);    
            cvedix_utils::put_text_at_center_of_rect(canvas, "output_fps: " + cvedix_utils::round_any(stream_status_hooker_storage.fps, 2),
                                                cv::Rect(node_left + node_rect.width / 2 - 10, node_top + node_title_h * 7 / 3 + node_queue_port_padding * 3, node_rect.width * 4 / 3 + 10, node_title_h * 2 / 3),
                                                true, font_face, 1, status_text_color, status_border_color, status_bg_color);   
            cvedix_utils::put_text_at_center_of_rect(canvas, "latency: " + std::to_string(stream_status_hooker_storage.latency) + "ms",
                                                cv::Rect(node_left + node_rect.width / 2 - 10, node_top + node_title_h * 9 / 3 + node_queue_port_padding * 4, node_rect.width * 4 / 3 + 10, node_title_h * 2 / 3),
                                                true, font_face, 1, status_text_color, status_border_color, status_bg_color);
        }

        // Flash connections if active
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - meta_leaving_hooker_storage.last_active_time);
        if (duration.count() < 100) { // 100ms flash duration
             // draw blocks connect line between nodes and nodes
            auto draw_connect_block = [=](int next_node_top){
                // Flashing Green Color
                cv::Scalar connection_color(0, 255, 0);  // Green

                cv::line(canvas, 
                            cv::Point(node_left + node_rect.width + node_queue_port_w_h, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                            cv::Point(node_left + node_rect.width + node_gap_horizontal / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2),
                            connection_color, 2, cv::LINE_AA);
                cv::line(canvas, 
                        cv::Point(node_left + node_rect.width + node_gap_horizontal / 2, node_top + node_title_h + node_queue_port_padding + node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_rect.width + node_gap_horizontal / 2, next_node_top + node_rect.height - node_queue_port_padding - node_queue_port_w_h / 2),
                        connection_color, 2, cv::LINE_AA);
                cv::line(canvas, 
                        cv::Point(node_left + node_rect.width + node_gap_horizontal / 2, next_node_top + node_rect.height - node_queue_port_padding - node_queue_port_w_h / 2), 
                        cv::Point(node_left + node_rect.width + node_gap_horizontal - node_queue_port_w_h, next_node_top + node_rect.height - node_queue_port_padding - node_queue_port_w_h / 2),
                        connection_color, 2, cv::LINE_AA);

                // Improved arrow with better color
                std::vector<cv::Point> vertexs {cv::Point(node_left + node_rect.width + node_gap_horizontal - node_queue_port_w_h, next_node_top + node_rect.height - node_queue_port_padding - node_queue_port_w_h / 2), 
                                                cv::Point(node_left + node_rect.width + node_gap_horizontal - node_queue_port_w_h * 2, next_node_top + node_rect.height - node_queue_port_padding - node_queue_port_w_h), 
                                                cv::Point(node_left + node_rect.width + node_gap_horizontal - node_queue_port_w_h * 2, next_node_top + node_rect.height - node_queue_port_padding)};
                cv::fillPoly(canvas, std::vector<std::vector<cv::Point>>{vertexs}, connection_color);};
            
            auto next_nodes_num = next_nodes_on_screen.size();
            for (int j = 0; j < next_nodes_num; j++) {
                draw_connect_block(next_nodes_on_screen[j]->node_rect.y);
            }   
        }
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_node_on_screen::get_latest_handled_meta() {
        std::lock_guard<std::mutex> guard(hooker_mutex);
        return meta_handled_hooker_storage.meta;
    }

    std::shared_ptr<cvedix_nodes::cvedix_node>& cvedix_node_on_screen::get_orginal_node() {
        return original_node;
    }

    std::vector<std::shared_ptr<cvedix_node_on_screen>>& cvedix_node_on_screen::get_next_nodes_on_screen() {
        return next_nodes_on_screen;
    }
}