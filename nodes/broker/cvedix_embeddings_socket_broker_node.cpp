

#include "cvedix_embeddings_socket_broker_node.h"


namespace cvedix_nodes {
        
    cvedix_embeddings_socket_broker_node::cvedix_embeddings_socket_broker_node(std::string node_name,
                                                        std::string des_ip,
                                                        int des_port,
                                                        std::string cropped_dir,
                                                        int min_crop_width,
                                                        int min_crop_height,
                                                        cvedix_broke_for broke_for,
                                                        bool only_for_tracked, 
                                                        int broking_cache_warn_threshold, 
                                                        int broking_cache_ignore_threshold):
                                                        cvedix_msg_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold),
                                                        des_ip(des_ip),
                                                        des_port(des_port),
                                                        cropped_dir(cropped_dir),
                                                        min_crop_width(min_crop_width),
                                                        min_crop_height(min_crop_height),
                                                        only_for_tracked(only_for_tracked) {
        // only for cvedix_frame_target or cvedix_frame_face_target                                                    
        assert(broke_for == cvedix_broke_for::NORMAL || broke_for == cvedix_broke_for::FACE);
        udp_writer = kissnet::udp_socket(kissnet::endpoint(des_ip, des_port));
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [message broker] set des_ip as `%s` and des_port as [%d]", node_name.c_str(), des_ip.c_str(), des_port));
        this->initialized();
    }
    
    cvedix_embeddings_socket_broker_node::~cvedix_embeddings_socket_broker_node() {
        deinitialized();
        stop_broking();
    }

    void cvedix_embeddings_socket_broker_node::format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) {
        /* format:
        line 0, <--
        line 1, 1st cropped image's path
        line 2, 1st embeddings
        line 3, -->
        line 4, <--
        line 5, 2nd cropped image's path
        line 6, 2nd embeddings
        line 7, -->
        line 8, ...
        */
        auto& broked = all_broked[meta->channel_index];
        // remove 50 elements every 100 ids
        if (broked.size() > 100) {
            broked.erase(broked.begin(), broked.begin() + 50);
        }
        
        std::stringstream msg_stream;
        auto format_embeddings = [&](const std::vector<float>& embeddings) {
            for (int i = 0; i < embeddings.size(); i++) {
                msg_stream << embeddings[i];
                if (i != embeddings.size() - 1) {
                    msg_stream << ",";
                }
            }
            msg_stream << std::endl;
        };
        auto save_cropped_image = [&](cv::Mat& frame, cv::Rect rect, std::string name) {
            auto cropped = frame(rect);
            cv::imwrite(name, cropped);
            msg_stream << name << std::endl;
        };

        if (broke_for == cvedix_broke_for::NORMAL) {
            for (int i = 0; i < meta->targets.size(); i++) {
                auto& t = meta->targets[i];
                // only broke for tracked targets and have enough frames
                if ((only_for_tracked && t->track_id < 0) || (only_for_tracked && t->tracks.size() < min_tracked_frames)) {
                    continue;
                }
                
                // only broke 1 time for specific track id if it has been tracked, or broke many times
                if (t->track_id >= 0 && 
                    std::find(broked.begin(), broked.end(), t->track_id) != broked.end()) {
                    continue;
                }
                
                // size filter
                if (t->width < min_crop_width || t->height < min_crop_height) {
                    continue;
                }

                if (t->track_id >= 0) {
                    broked.push_back(t->track_id);
                }
                auto name = cropped_dir + "/" + std::to_string(t->channel_index) + "_" + std::to_string(t->frame_index) + "_" + std::to_string(t->track_id >= 0 ? t->track_id : i) + ".jpg";
                // start flag
                msg_stream << "<--" << std::endl;
                // save small cropped image
                save_cropped_image(meta->frame, cv::Rect(t->x, t->y, t->width, t->height), name);
                // format embeddings
                format_embeddings(t->embeddings);
                // end flag
                msg_stream << "-->";
                
                if (i != meta->targets.size() - 1) {
                    msg_stream << std::endl;  // not the last one
                }
            }
        }

        if (broke_for == cvedix_broke_for::FACE) {
            for (int i = 0; i < meta->face_targets.size(); i++) {
                auto& t = meta->face_targets[i];
                // only broke for tracked targets and have enough frames
                if ((only_for_tracked && t->track_id < 0) || (only_for_tracked && t->tracks.size() < min_tracked_frames)) {
                    continue;
                }

                // only broke 1 time for specific track id if it has been tracked, or broke many times
                if (t->track_id >= 0 && 
                    std::find(broked.begin(), broked.end(), t->track_id) != broked.end()) {
                    continue;
                }

                // size filter
                if (t->width < min_crop_width || t->height < min_crop_height) {
                    continue;
                }
                
                if (t->track_id >= 0) {
                    broked.push_back(t->track_id);
                }
                auto name = cropped_dir + "/" + std::to_string(meta->channel_index) + "_" + std::to_string(meta->frame_index) + "_" + std::to_string(t->track_id >= 0 ? t->track_id : i) + ".jpg";
                // start flag
                msg_stream << "<--" << std::endl;
                // save small cropped image
                save_cropped_image(meta->frame, cv::Rect(t->x, t->y, t->width, t->height), name);
                // format embeddings
                format_embeddings(t->embeddings);
                // end flag
                msg_stream << "-->";

                if (i != meta->face_targets.size() - 1) {
                    msg_stream << std::endl;  // not the last one
                }
            }
        }
        
        msg = msg_stream.str();   
    }

    void cvedix_embeddings_socket_broker_node::broke_msg(const std::string& msg) {
        // broke msg to socket by udp
        auto bytes_2_send = reinterpret_cast<const std::byte*>(msg.c_str());
        auto bytes_2_send_len = msg.size();
        udp_writer.send(bytes_2_send, bytes_2_send_len);
    }
}