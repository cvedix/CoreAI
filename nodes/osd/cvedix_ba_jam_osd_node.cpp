
#include "cvedix_ba_jam_osd_node.h"

namespace cvedix_nodes {
    
    cvedix_ba_jam_osd_node::cvedix_ba_jam_osd_node(std::string node_name, std::string font): cvedix_node(node_name) {
        if (!font.empty()) {
            ft2 = cv::freetype::createFreeType2();
            ft2->loadFontData(font, 0);   
        }      
        this->initialized();
    }
    
    cvedix_ba_jam_osd_node::~cvedix_ba_jam_osd_node() {
        deinitialized();
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ba_jam_osd_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        // operations on osd_frame
        if (meta->osd_frame.empty()) {
            meta->osd_frame = meta->frame.clone();
        }

        auto& canvas = meta->osd_frame;
        // scan targets
        for (auto& i : meta->targets) {
            // track_id
            auto id = std::to_string(i->track_id);
            auto labels_to_display = i->primary_label;

            // tracked
            if (i->track_id != -1) {
                labels_to_display = "#" + id + " " + labels_to_display;
            }
            
            for (auto& label : i->secondary_labels) {
                labels_to_display += "|" + label;
            }
            
            // draw tracks if size>=2
            if (i->tracks.size() >= 2) {
                for (int n = 0; n < (i->tracks.size() - 1); n++) {
                    auto p1 = i->tracks[n].track_point(cvedix_objects::cvedix_rect_anchor_point::CENTER);
                    auto p2 = i->tracks[n + 1].track_point(cvedix_objects::cvedix_rect_anchor_point::CENTER);
                    cv::line(canvas, cv::Point(p1.x, p1.y), cv::Point(p2.x, p2.y), cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
                }
            }

            cv::rectangle(canvas, cv::Rect(i->x, i->y, i->width, i->height), cv::Scalar(255, 255, 0), 2);
            if (ft2 != nullptr) {
                ft2->putText(canvas, labels_to_display, cv::Point(i->x, i->y), 20, cv::Scalar(255, 0, 255), cv::FILLED, cv::LINE_AA, true);
            }
            else {               
                //cv::putText(canvas, labels_to_display, cv::Point(i->x, i->y), 1, 1, cv::Scalar(255, 0, 255));
                int baseline = 0;
                auto size = cv::getTextSize(labels_to_display, 1, 1, 1, &baseline);
                cvedix_utils::put_text_at_center_of_rect(canvas, labels_to_display, cv::Rect(i->x, i->y - size.height, size.width, size.height), true, 1, 1, cv::Scalar(), cv::Scalar(179, 52, 255), cv::Scalar(179, 52, 255));
            }

            // scan sub targets
            for (auto& sub_target: i->sub_targets) {
                cv::rectangle(canvas, cv::Rect(sub_target->x, sub_target->y, sub_target->width, sub_target->height), cv::Scalar(255));
                if (ft2 != nullptr) {
                    ft2->putText(canvas, sub_target->label, cv::Point(sub_target->x, sub_target->y), 20, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA, true);
                }
                else {
                    cv::putText(canvas, sub_target->label, cv::Point(sub_target->x, sub_target->y), 1, 1, cv::Scalar(0, 0, 255));
                }
            }
        }
        
        /* jam draw for current channel */    
        auto& region = all_jam_regions[meta->channel_index];
        auto& jam_result = all_jam_results[meta->channel_index];
        auto& involve_ids = all_involve_ids[meta->channel_index];

        // scan ba results and ONLY deal with stop and unstop
        for (auto& i : meta->ba_results) {
            if (i->type == cvedix_objects::cvedix_ba_type::JAM) {
                region = i->involve_region_in_frame;
                involve_ids = i->involve_target_ids_in_frame;
                jam_result = true;
            }
            if (i->type == cvedix_objects::cvedix_ba_type::UNJAM) {
                region = i->involve_region_in_frame;
                involve_ids = i->involve_target_ids_in_frame;  // not used later 
                jam_result = false;
            }
        }

        // draw jam data
        auto poly_vertexs = [&]() {
            std::vector<cv::Point> vertexs;
            for(auto& p: region) {
                vertexs.push_back(cv::Point(p.x, p.y));
            }
            return vertexs;
        };
        if (jam_result) {
            cv::polylines(canvas, poly_vertexs(), true, cv::Scalar(0, 0, 255), 2, cv::LINE_AA); // region
            // targets in jan region, highlight
            auto targets = meta->get_targets_by_ids(involve_ids);
            for (auto& t: targets) {
                cv::rectangle(canvas,  cv::Rect(t->x, t->y, t->width, t->height), cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
            }
        }
        return meta;
    }
}