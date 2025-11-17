#include "cvedix_ba_result.h"


namespace cvedix_objects {

    cvedix_ba_result::cvedix_ba_result(cvedix_ba_type type, 
                    int channel_index,
                    int frame_index,
                    std::vector<int> involve_target_ids_in_frame, 
                    std::vector<cvedix_objects::cvedix_point> involve_region_in_frame,
                    std::string ba_label,
                    std::string record_image_name,
                    std::string record_video_name):
                    type(type), channel_index(channel_index), frame_index(frame_index),
                    involve_target_ids_in_frame(involve_target_ids_in_frame),
                    involve_region_in_frame(involve_region_in_frame),
                    ba_label(ba_label),
                    record_image_name(record_image_name),
                    record_video_name(record_video_name) {
        
    }

    cvedix_ba_result::~cvedix_ba_result() {

    }

    std::string cvedix_ba_result::to_string() {
        return "";
    }

    std::shared_ptr<cvedix_ba_result> cvedix_ba_result::clone() {
        return std::make_shared<cvedix_ba_result>(*this);
    }
}