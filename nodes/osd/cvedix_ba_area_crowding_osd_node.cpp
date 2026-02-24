#include "cvedix_ba_area_crowding_osd_node.h"

namespace cvedix_nodes {

    cvedix_ba_area_crowding_osd_node::cvedix_ba_area_crowding_osd_node(std::string node_name, std::string font): cvedix_node(node_name) {
        if (!font.empty()) {
            ft2 = cv::freetype::createFreeType2();
            ft2->loadFontData(font, 0);
        }
        this->initialized();
    }

    cvedix_ba_area_crowding_osd_node::~cvedix_ba_area_crowding_osd_node() {
        deinitialized();
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ba_area_crowding_osd_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        if (meta->osd_frame.empty()) {
            meta->osd_frame = meta->frame.clone();
        }
        auto& canvas = meta->osd_frame;

        // draw tracked targets (reuse common drawing from other OSD nodes)
        for (auto& i : meta->targets) {
            auto id = std::to_string(i->track_id);
            auto labels_to_display = i->primary_label;
            if (i->track_id != -1) {
                labels_to_display = "#" + id + " " + labels_to_display;
            }
            for (auto& label : i->secondary_labels) {
                labels_to_display += "|" + label;
            }

            if (i->tracks.size() >= 2) {
                for (int n = 0; n < (i->tracks.size() - 1); n++) {
                    auto p1 = i->tracks[n].track_point();
                    auto p2 = i->tracks[n + 1].track_point();
                    cv::line(canvas, cv::Point(p1.x, p1.y), cv::Point(p2.x, p2.y), cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
                }
            }

            cv::rectangle(canvas, cv::Rect(i->x, i->y, i->width, i->height), cv::Scalar(255, 255, 0), 2);
            if (ft2 != nullptr) {
                ft2->putText(canvas, labels_to_display, cv::Point(i->x, i->y), 20, cv::Scalar(255, 0, 255), cv::FILLED, cv::LINE_AA, true);
            }
            else {
                int baseline = 0;
                auto size = cv::getTextSize(labels_to_display, 1, 1, 1, &baseline);
                cvedix_utils::put_text_at_center_of_rect(canvas, labels_to_display, cv::Rect(i->x, i->y - size.height, size.width, size.height), true, 1, 1, cv::Scalar(), cv::Scalar(179, 52, 255), cv::Scalar(179, 52, 255));
            }

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

        // Process BA results: look for results with label "crowding"
        for (auto& i : meta->ba_results) {
            if (i->ba_label == "crowding") {
                // store last seen region and involved ids
                auto& region = i->involve_region_in_frame;
                std::vector<cv::Point> pts;
                for (auto& p: region) pts.emplace_back(cv::Point(p.x, p.y));
                // draw ROI polygon (should be rectangle)
                if (!pts.empty()) {
                    cv::polylines(canvas, pts, true, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
                }
                // highlight involve targets
                auto targets = meta->get_targets_by_ids(i->involve_target_ids_in_frame);
                for (auto& t: targets) {
                    cv::rectangle(canvas, cv::Rect(t->x, t->y, t->width, t->height), cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
                }

                // draw corner alert (top-right)
                std::string alert = "CROWDING";
                int fontFace = cv::FONT_HERSHEY_SIMPLEX;
                double fontScale = 1.0;
                int thickness = 2;
                int baseline = 0;
                auto textSize = cv::getTextSize(alert, fontFace, fontScale, thickness, &baseline);
                int padding = 10;
                cv::Point org(canvas.cols - textSize.width - padding, padding + textSize.height);
                // background box
                cv::rectangle(canvas, cv::Point(org.x - padding/2, org.y - textSize.height - padding/2), cv::Point(org.x + textSize.width + padding/2, org.y + padding/2), cv::Scalar(0,0,255), cv::FILLED);
                // text
                cv::putText(canvas, alert, org, fontFace, fontScale, cv::Scalar(255,255,255), thickness);
                // draw count under the alert
                int count = static_cast<int>(i->involve_target_ids_in_frame.size());
                std::string count_str = cvedix_utils::string_format("Count: %d", count);
                double countScale = 0.8;
                int countTh = 2;
                int countBaseline = 0;
                auto countSize = cv::getTextSize(count_str, fontFace, countScale, countTh, &countBaseline);
                cv::Point countOrg(org.x, org.y + textSize.height + padding);
                cv::rectangle(canvas, cv::Point(countOrg.x - padding/2, countOrg.y - countSize.height - padding/2), cv::Point(countOrg.x + countSize.width + padding/2, countOrg.y + padding/2), cv::Scalar(0,0,255), cv::FILLED);
                cv::putText(canvas, count_str, countOrg, fontFace, countScale, cv::Scalar(255,255,255), countTh);
            }
        }

        return meta;
    }
}
