

#include "cvedix_meta_publisher.h"

namespace cvedix_nodes {
    cvedix_meta_publisher::cvedix_meta_publisher(/* args */) {

    }

    cvedix_meta_publisher::~cvedix_meta_publisher() {

    }

    void cvedix_meta_publisher::add_subscriber(std::shared_ptr<cvedix_meta_subscriber> subscriber) {
        std::lock_guard<std::mutex> guard(this->subscribers_lock);
        this->subscribers.push_back(subscriber);
    }

    void cvedix_meta_publisher::remove_subscriber(std::shared_ptr<cvedix_meta_subscriber> subscriber) {
        std::lock_guard<std::mutex> guard(this->subscribers_lock);
        for (auto i = this->subscribers.begin(); i != this->subscribers.end();) {
            if(*i == subscriber) {
                i = this->subscribers.erase(i);
            }
            else {
                i++;
            }
        }     
    }

    // by default, we push meta to next nodes indiscriminately, each next node has the same meta pointer.
    // in some situations, we need push meta depend on condition, refer to cvedix_split_node which would push meta by channel index or push a deep copy pf meta(new pointer to new memory). 
    void cvedix_meta_publisher::push_meta(std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
        std::lock_guard<std::mutex> guard(this->subscribers_lock);
        for (auto i = this->subscribers.begin(); i != this->subscribers.end(); i++) {
            (*i)->meta_flow(meta);
        }
    }
}