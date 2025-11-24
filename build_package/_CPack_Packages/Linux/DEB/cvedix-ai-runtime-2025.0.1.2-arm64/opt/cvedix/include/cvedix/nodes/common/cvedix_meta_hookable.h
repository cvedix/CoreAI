#pragma once
#include <functional>
#include <mutex>
#include <string>
#include <memory>

#include "cvedix/objects/cvedix_meta.h"

namespace cvedix_nodes {
    // callback when meta flowing through the whole pipe, MUST NOT be blocked.
    // we can do more work based on this callback, such as calculating fps/latency at each port of node, please refer to cvedix_analysis_board for details.
    typedef std::function<void(std::string, int, std::shared_ptr<cvedix_objects::cvedix_meta>)> cvedix_meta_hooker;

    // allow hookers attached to the pipe (nodes), get notified when meta flow through each port of node (total 4 ports in node).
    // this class is inherited by cvedix_node only.
    class cvedix_meta_hookable {
    protected:
        std::mutex meta_arriving_hooker_lock;
        std::mutex meta_handling_hooker_lock;
        std::mutex meta_handled_hooker_lock;
        std::mutex meta_leaving_hooker_lock;
        // hooker activated when meta is arriving at node (pushed to in_queue of cvedix_node, the 1st port in node).
        cvedix_meta_hooker meta_arriving_hooker;
        // hooker activated when meta is to be handled inside node (poped from in_queue of cvedix_node, the 2nd port in node).
        cvedix_meta_hooker meta_handling_hooker;
        // hooker activated when meta is handled inside node (pushed to out_queue of cvedix_node, the 3rd port in node). 
        cvedix_meta_hooker meta_handled_hooker;
        // hooker activated when meta is leaving from node (poped from out_queue of cvedix_node, the 4th port in node).
        cvedix_meta_hooker meta_leaving_hooker;
    public:
        cvedix_meta_hookable(/* args */) {}
        ~cvedix_meta_hookable() {}

        void set_meta_arriving_hooker(cvedix_meta_hooker meta_arriving_hooker) {
            std::lock_guard<std::mutex> guard(meta_arriving_hooker_lock);
            this->meta_arriving_hooker = meta_arriving_hooker;
        }

        void set_meta_handling_hooker(cvedix_meta_hooker meta_handling_hooker) {
            std::lock_guard<std::mutex> guard(meta_handling_hooker_lock);
            this->meta_handling_hooker = meta_handling_hooker;
        }

        void set_meta_handled_hooker(cvedix_meta_hooker meta_handled_hooker) {
            std::lock_guard<std::mutex> guard(meta_handled_hooker_lock);
            this->meta_handled_hooker = meta_handled_hooker;
        }

        void set_meta_leaving_hooker(cvedix_meta_hooker meta_leaving_hooker) {
            std::lock_guard<std::mutex> guard(meta_leaving_hooker_lock);
            this->meta_leaving_hooker = meta_leaving_hooker;
        }

        void invoke_meta_arriving_hooker(std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
            std::lock_guard<std::mutex> guard(meta_arriving_hooker_lock);
            if (this->meta_arriving_hooker) {
                this->meta_arriving_hooker(node_name, queue_size, meta);
            }
        }

        void invoke_meta_handling_hooker(std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
            std::lock_guard<std::mutex> guard(meta_handling_hooker_lock);
            if (this->meta_handling_hooker) {
                this->meta_handling_hooker(node_name, queue_size, meta);
            }
        }

        void invoke_meta_handled_hooker(std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
            std::lock_guard<std::mutex> guard(meta_handled_hooker_lock);
            if (this->meta_handled_hooker) {
                this->meta_handled_hooker(node_name, queue_size, meta);
            }
        }

        void invoke_meta_leaving_hooker(std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
            std::lock_guard<std::mutex> guard(meta_leaving_hooker_lock);
            if (this->meta_leaving_hooker) {
                this->meta_leaving_hooker(node_name, queue_size, meta);
            }
        }
    };
}