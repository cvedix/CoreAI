#pragma once

#include "cvedix_node.h"
#include "cvedix_stream_info_hookable.h"
#include "cvedix/excepts/cvedix_not_implemented_error.h"
#include "cvedix/excepts/cvedix_invalid_calling_error.h"
#include "cvedix/utils/cvedix_gate.h"

namespace cvedix_nodes {
    // base class for src nodes, start point of meta/pipeline
    class cvedix_src_node: public cvedix_node, public cvedix_stream_info_hookable {
    private:
        /* data */
    
    protected:
        // force re-implemetation in child class
        virtual void handle_run() override;
        // force ignored in child class
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override; 
        // force ignored in child class
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

        // protected as it can't be instanstiated directly.
        cvedix_src_node(std::string node_name, 
                    int channel_index, 
                    float resize_ratio = 1.0);

        // basic stream info
        int original_fps = -1;
        int original_width = 0;
        int original_height = 0;

        // basic channel info
        int frame_index;
        int channel_index;
        float resize_ratio;

        // control to work or not
        // all derived class need depend on the value to check if work or not (start/stop)
        cvedix_utils::cvedix_gate gate;

        // new logic for sending dead flag
        virtual void deinitialized() override;
    public:
        ~cvedix_src_node();

        virtual cvedix_node_type node_type() override;

        // start signal to pipeline
        void start();
        // stop signal to pipeline
        void stop();
        // speak signal to the pipeline (each node print some message such as current status)
        void speak();

        /* Debug API */
        // record video signal to pipeline, start recording video when cvedix_record_node(if exists) receive the signal.
        // it is a debug api since record signal is usually generated automatically inside pipe(not by users outside pipe).
        void record_video_manually(bool osd = false, int video_duration = 10);
        /* Debug API */
        // record image signal to pipeline, start recording image when cvedix_record_node(if exists) receive the signal.
        // it is a debug api since record signal is usually generated automatically inside pipe(not by userd outside pipe).
        void record_image_manually(bool osd = false);

        int get_original_fps() const;
        int get_original_width() const;
        int get_original_height() const;
    };
    
}