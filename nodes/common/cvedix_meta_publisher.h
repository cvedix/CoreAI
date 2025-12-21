/**
 * @file cvedix_meta_publisher.h
 * @brief Publisher interface for the publish-subscribe pattern
 * 
 * This file defines the cvedix_meta_publisher class which enables nodes
 * to push metadata to downstream subscribers.
 * 
 * @see cvedix_meta_subscriber Subscriber counterpart
 */

#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <iostream>

#include <iostream>

#include "cvedix_meta_subscriber.h"

namespace cvedix_nodes {
    class cvedix_meta_publisher {
    private:
    protected:
        // push meta to next nodes
        virtual void push_meta(std::shared_ptr<cvedix_objects::cvedix_meta> meta);

        // non-copyable for all child class
        std::mutex subscribers_lock;
        // next nodes as subscribers
        std::vector<std::shared_ptr<cvedix_meta_subscriber>> subscribers;
    public:
        cvedix_meta_publisher(/* args */);
        ~cvedix_meta_publisher();

        // add next node
        void add_subscriber(std::shared_ptr<cvedix_meta_subscriber> subscriber);
        // remove next node
        void remove_subscriber(std::shared_ptr<cvedix_meta_subscriber> subscriber);
    };

}

