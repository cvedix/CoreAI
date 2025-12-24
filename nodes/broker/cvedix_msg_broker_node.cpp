
#include "cvedix_msg_broker_node.h"


namespace cvedix_nodes {
        
    cvedix_msg_broker_node::cvedix_msg_broker_node(std::string node_name,
                                        cvedix_broke_for broke_for,
                                        int broking_cache_warn_threshold, 
                                        int broking_cache_ignore_threshold): 
                                        cvedix_node(node_name), 
                                        broke_for(broke_for),
                                        broking_cache_warn_threshold(broking_cache_warn_threshold),
                                        broking_cache_ignore_threshold(broking_cache_ignore_threshold) {
        this->initialized();
        broking_th = std::thread(&cvedix_msg_broker_node::broking_run, this);
    }
    
    cvedix_msg_broker_node::~cvedix_msg_broker_node() {

    }

    void cvedix_msg_broker_node::stop_broking() {
        broking = false;
        frames_to_broke.push(nullptr);   // send dead flag to broking_thread
        broking_cache_semaphore.signal();
        if (broking_th.joinable()) {
            broking_th.join();
        }
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_msg_broker_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        // cache frame meta only if cache size is not greater than threshold
        if (frames_to_broke.size() < broking_cache_ignore_threshold) {
            // it is a producer
            frames_to_broke.push(meta);
            broking_cache_semaphore.signal();
        }

        // warning 1 time in log
        auto size = frames_to_broke.size();
        if (size > broking_cache_warn_threshold && !broking_cache_warned) {
            broking_cache_warned = true;
            CVEDIX_WARN(cvedix_utils::string_format("[%s] [message broker] cache size is exceeding threshold! cache size is [%d], threshold is [%d]", node_name.c_str(), size, broking_cache_warn_threshold));
        }

        if (size <= broking_cache_warn_threshold) {
            broking_cache_warned = false;
        }

        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_msg_broker_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
        return meta;
    }

    void cvedix_msg_broker_node::broking_run() {
        while (broking) {
            // it is a consumer
            broking_cache_semaphore.wait();

            auto frame_meta = frames_to_broke.front();
            frames_to_broke.pop();

            // dead flag
            if (frame_meta == nullptr) {
                continue;
            }
            
            // message to be broked
            std::string message;

            // step 1, format message
            format_msg(frame_meta, message);  // MUST be implemented in child class

            // ignore if message is empty, because no broking occurs is allowed for some frames if some conditions not satisfied
            if (message.empty()) {
                continue;
            }
            
            // step 2, broke message
            broke_msg(message);               // MUST be implemented in child class
        }
    }
}