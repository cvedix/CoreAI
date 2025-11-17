#include <iterator>

#include "cvedix_frame_meta.h"

namespace cvedix_objects {
        
    cvedix_frame_meta::cvedix_frame_meta(cv::Mat frame, int frame_index, int channel_index, int original_width, int original_height, int fps): 
        cvedix_meta(cvedix_meta_type::FRAME, channel_index), 
        frame_index(frame_index), 
        original_width(original_width),
        original_height(original_height),
        fps(fps),
        frame(frame) {
            assert(!frame.empty());
    }
    
    // copy constructor of cvedix_frame_meta would NOT be called at most time.
    // only when it flow through cvedix_split_node with cvedix_split_node::split_with_deep_copy==true.
    // in fact, all kinds of meta would NOT be copyed in its lifecycle, we just pass them by poniter most time.
    cvedix_frame_meta::cvedix_frame_meta(const cvedix_frame_meta& meta): 
        cvedix_meta(meta),
        frame_index(meta.frame_index),
        original_width(meta.original_width),
        original_height(meta.original_height),
        description(meta.description),
        fps(meta.fps) {
            // deep copy frame data
            this->frame = meta.frame.clone();
            this->osd_frame = meta.osd_frame.clone();
            this->mask = meta.mask.clone();

            // deep copy targets
            for(auto& i: meta.targets) {
                this->targets.push_back(i->clone());
            }
            // deep copy pose targets
            for(auto& i: meta.pose_targets) {
                this->pose_targets.push_back(i->clone());
            }
            // deep copy face targets
            for(auto& i: meta.face_targets) {
                this->face_targets.push_back(i->clone());
            }
            // deep copy text targets
            for(auto& i: meta.text_targets) {
                this->text_targets.push_back(i->clone());
            }
            // deep copy ba results
            for(auto& i: meta.ba_results) {
                this->ba_results.push_back(i->clone());
            }
    }
    
    cvedix_frame_meta::~cvedix_frame_meta() {

    }

    std::shared_ptr<cvedix_meta> cvedix_frame_meta::clone() {
        // just call copy constructor and return new pointer
        return std::make_shared<cvedix_frame_meta>(*this);
    }

    std::vector<std::shared_ptr<cvedix_frame_target>> cvedix_frame_meta::get_targets_by_ids(const std::vector<int>& ids) {
        std::vector<std::shared_ptr<cvedix_objects::cvedix_frame_target>> results;
        for(auto& t: targets) {
            if (std::find(ids.begin(), ids.end(), t->track_id) != ids.end()) {
                results.push_back(t);
            }
        }
        return results;
    }
}