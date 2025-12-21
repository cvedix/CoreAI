/**
 * @file cvedix_control_meta.h
 * @brief Control metadata for pipeline commands
 * 
 * Triggers actions like recording, configuration changes.
 */

#pragma once

#include <chrono>
#include "cvedix_meta.h"

namespace cvedix_objects {
    /** @brief Control command types */
    enum cvedix_control_type {
        SPEAK,
        VIDEO_RECORD,
        IMAGE_RECORD
    };

    /**
     * @brief Control metadata for commands
     */
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