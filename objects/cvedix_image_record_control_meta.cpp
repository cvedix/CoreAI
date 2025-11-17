#include "cvedix_image_record_control_meta.h"

namespace cvedix_objects {
        
    cvedix_image_record_control_meta::cvedix_image_record_control_meta(int channel_index, std::string image_file_name_without_ext, bool osd): 
                                                                cvedix_control_meta(cvedix_control_type::IMAGE_RECORD, channel_index),
                                                                image_file_name_without_ext(image_file_name_without_ext),
                                                                osd(osd) {
    }
    
    cvedix_image_record_control_meta::~cvedix_image_record_control_meta() {

    }

    std::shared_ptr<cvedix_meta> cvedix_image_record_control_meta::clone() {
        // just call copy constructor and return new pointer
        return std::make_shared<cvedix_image_record_control_meta>(*this);
    }
}