#pragma once

#include <fstream>

#include "cvedix_msg_broker_node.h"
#include "cereal_archive/cvedix_objects_cereal_archive.h"

namespace cvedix_nodes {
    // message broker node (for demo/debug purpose), broke xml data to file.
    class cvedix_xml_file_broker_node: public cvedix_msg_broker_node
    {
    private:
        // xml file writer
        ofstream xml_writer;
    protected:
        // to xml
        virtual void format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) override;
        // to file
        virtual void broke_msg(const std::string& msg) override;
    public:
        cvedix_xml_file_broker_node(std::string node_name, 
                                cvedix_broke_for broke_for = cvedix_broke_for::NORMAL, 
                                std::string file_path_and_name = "",
                                int broking_cache_warn_threshold = 50, 
                                int broking_cache_ignore_threshold = 200);
        ~cvedix_xml_file_broker_node();
    };
}