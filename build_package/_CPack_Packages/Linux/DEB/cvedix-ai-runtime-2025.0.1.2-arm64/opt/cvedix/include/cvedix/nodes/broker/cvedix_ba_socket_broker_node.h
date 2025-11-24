#pragma once

#include "cvedix_msg_broker_node.h"
#include "cvedix/objects/ba/cvedix_ba_result.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

// light weight socket support
#include "../../third_party/kissnet/kissnet.hpp"

namespace cvedix_nodes {
    // message broker node, broke BA results (ONLY for cvedix_frame_target) to socket via udp.
    // BA results could be used for archive.
    class cvedix_ba_socket_broker_node: public cvedix_msg_broker_node
    {
    private:
        // host the data sent to via udp
        std::string des_ip = "";
        // port the data sent to via udp
        int des_port = 0;

        // udp socket writer
        kissnet::udp_socket udp_writer;
    protected:
        // to custom format
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        // to socket via udp
        virtual void broke_msg(const std::string& msg) override;
    public:
        cvedix_ba_socket_broker_node(std::string node_name, 
                                std::string des_ip = "",
                                int des_port = 0,
                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);
        ~cvedix_ba_socket_broker_node();
    };
}