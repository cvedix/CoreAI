#pragma once

#include <iostream>
#include <memory>
#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "../common/cvedix_des_node.h"
#include "cvedix/objects/cvedix_frame_meta.h"
#include "cvedix/objects/cvedix_control_meta.h"
#include "cvedix/utils/cvedix_utils.h"


namespace cvedix_nodes {
    // callback before data disappear inside cvedix_app_des_node
    typedef std::function<void(std::string, std::shared_ptr<cvedix_objects::cvedix_meta>)> cvedix_app_des_result_hooker;

    // app des node, send meta data to external host code using callbacks.
    class cvedix_app_des_node: public cvedix_des_node {
    private:
        /* data */
        cvedix_app_des_result_hooker app_des_result_hooker;

        void invoke_app_des_result_hooker(std::shared_ptr<cvedix_objects::cvedix_meta> meta);
    protected:
        // re-implementation, return nullptr.
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 
        // re-implementation, return nullptr.
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_app_des_node(std::string node_name, 
                        int channel_index);
        ~cvedix_app_des_node();

        void set_app_des_result_hooker(cvedix_app_des_result_hooker app_des_result_hooker);
    };
}