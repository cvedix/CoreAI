#pragma once

#include <chrono>
#include "cvedix_meta.h"


namespace cvedix_objects {
    // type of control meta
    enum cvedix_control_type {
        SPEAK,
        VIDEO_RECORD,
        IMAGE_RECORD
    };

    // control meta, which contains control data.
    class cvedix_control_meta: public cvedix_meta {
    private:
        // help to generate control uid if need
        void generate_uid();
    public:
        cvedix_control_meta(cvedix_control_type control_type, int channel_index, std::string control_uid = "");
        ~cvedix_control_meta();

        cvedix_control_type control_type;
        // unique id to identify control meta (caould be generated in random)
        std::string control_uid;

        // copy myself
        virtual std::shared_ptr<cvedix_meta> clone() override;
    };

}