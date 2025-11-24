#pragma once

#include "../common/cvedix_node.h"

namespace cvedix_nodes {
    // math expression checker, give right for `1+1=2` and wrong for `sqrt(4)=4`.
    // note: this node works based on cvedix_frame_text_target, it will parse expression at the left of `=` and calculate it then compare with the right side of `=` .
    class cvedix_expr_check_node: public cvedix_node
    {
    private:
    protected:
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    public:
        cvedix_expr_check_node(std::string node_name);
        ~cvedix_expr_check_node();
    };
}