/**
 * @file cvedix_expr_check_node.h
 * @brief Math expression checker (OCR validation)
 * 
 * Parses and validates math expressions from OCR results.
 * Example: `1+1=2` → right, `sqrt(4)=4` → wrong
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Math expression validation node
     */
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