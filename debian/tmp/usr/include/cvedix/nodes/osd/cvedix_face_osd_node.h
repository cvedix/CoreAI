#pragma once

#include "../common/cvedix_node.h"

namespace cvedix_nodes {
    // on screen display(short as osd) node.
    // mainly used to display cvedix_frame_face_target on frame.
    class cvedix_face_osd_node: public cvedix_node
    {
    private:
        /* data */
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_face_osd_node(std::string node_name);
        ~cvedix_face_osd_node();
    };

}