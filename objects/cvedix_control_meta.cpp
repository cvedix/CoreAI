
#include "cvedix_control_meta.h"

namespace cvedix_objects {
        
    cvedix_control_meta::cvedix_control_meta(cvedix_control_type control_type, int channel_index, std::string control_uid): 
        cvedix_meta(cvedix_meta_type::CONTROL, channel_index), control_type(control_type), control_uid(control_uid) {
            if (control_uid.empty()){
                generate_uid();
            }
    }
    
    cvedix_control_meta::~cvedix_control_meta() {

    }

    std::shared_ptr<cvedix_meta> cvedix_control_meta::clone() {
        // just call copy constructor and return new pointer
        return std::make_shared<cvedix_control_meta>(*this);
    }

    void cvedix_control_meta::generate_uid() {
        auto now = std::chrono::system_clock::now();
        auto period = now.time_since_epoch();

        // milliseconds since 1970-1-1 00:00:00
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(period).count();

        control_uid = "cvedix_control_" + std::to_string(timestamp);
    }
}