#pragma once

#include <sstream>

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {
    // message broker node (for debug purpose), broke json data to console.
    class cvedix_console_broker_node: public cvedix_msg_broker_node
    {
    private:
        /* data */
    protected:
        // to json
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        // to console
        virtual void broke_msg(const std::string& msg) override;
    public:
        cvedix_console_broker_node(std::string node_name, 
                                    cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                    int broking_cache_warn_threshold = 50, 
                                    int broking_cache_ignore_threshold = 200);
        ~cvedix_console_broker_node();
    };
}