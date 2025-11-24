#pragma once

#include <memory>
#include "cvedix/objects/cvedix_meta.h"

namespace cvedix_nodes {
    class cvedix_meta_subscriber {
    private:
        /* data */
    public:
        cvedix_meta_subscriber(/* args */);
        ~cvedix_meta_subscriber();

        // non-copyable for all child class
        cvedix_meta_subscriber(const cvedix_meta_subscriber&) = delete;
        cvedix_meta_subscriber& operator=(const cvedix_meta_subscriber&) = delete;

        // receive meta from previous nodes
        virtual void meta_flow(std::shared_ptr<cvedix_objects::cvedix_meta> meta) = 0;
    };

}
