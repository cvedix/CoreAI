
#include "cvedix_file_des_node.h"
#include "cvedix/nodes/common/frame_utils.h"

namespace cvedix_nodes {
        
    cvedix_file_des_node::cvedix_file_des_node(std::string node_name, 
                                        int channel_index, 
                                        std::string save_dir,
                                        std::string name_prefix,
                                        int max_duration_for_single_file,
                                        cvedix_objects::cvedix_size resolution_w_h,
                                        int bite_rate,
                                        bool osd,
                                        std::string gst_encoder_name): 
                                        cvedix_des_node(node_name, channel_index), 
                                        save_dir(save_dir),
                                        name_prefix(name_prefix),
                                        max_duration_for_single_file(max_duration_for_single_file),
                                        resolution_w_h(resolution_w_h),
                                        bitrate(bitrate),
                                        osd(osd),
                                        gst_encoder_name(gst_encoder_name) {
        // compile tips:
        // remove experimental:: if gcc >= 8.0
        assert(std::experimental::filesystem::exists(save_dir));
        // max time to record
        assert(max_duration_for_single_file <= 30);
        CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), gst_template.c_str()));
        this->initialized();
    }
    
    cvedix_file_des_node::~cvedix_file_des_node() {
        deinitialized();
    }
    
    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_file_des_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] received frame meta, channel_index=>%d, frame_index=>%d", node_name.c_str(), meta->channel_index, meta->frame_index));
            
            auto resize_frame = utils::prepare_output_frame(meta, osd, resolution_w_h);

            // new video file
            if (!file_writer.isOpened() ||
                frames_already_record >= frames_need_record) {
                    // total frames need to be recorded
                    frames_need_record = max_duration_for_single_file * 60 * meta->fps;
                    frames_already_record = 0;

                    if (name_prefix.empty()) {
                        name_prefix = node_name + "_" + std::to_string(meta->channel_index);
                    }

                    // close it first if it has opened
                    if (file_writer.isOpened()) {
                        /* code */
                        file_writer.release();
                    }
                    
                    auto gst_str = cvedix_utils::string_format(this->gst_template, gst_encoder_name.c_str(), bitrate, get_new_file_name().c_str());
                    assert(file_writer.open(gst_str, cv::CAP_GSTREAMER, 0, meta->fps, {resize_frame.cols, resize_frame.rows}));
            }
            
            file_writer.write(resize_frame);
            frames_already_record++;

            // for general works defined in base class
            return cvedix_des_node::handle_frame_meta(meta);
    }

    // re-implementation, return nullptr.
    std::shared_ptr<cvedix_objects::cvedix_meta> 
        cvedix_file_des_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
            return nullptr;
    }

    std::string cvedix_file_des_node::get_new_file_name() {
        auto stamp = cvedix_utils::time_format(NOW, "<year>-<mon>-<day>_<hour>-<min>-<sec>-<mili>");
        // compile tips:
        // remove experimental:: if gcc >= 8.0
        std::experimental::filesystem::path p1(save_dir);
        std::experimental::filesystem::path p2(name_prefix + "_" + stamp + ".mp4");

        // save_dir/name_prefix_stamp.mp4
        std::experimental::filesystem::path p = p1 / p2;

        assert(!std::experimental::filesystem::exists(p));
        return p.string();
    }
}