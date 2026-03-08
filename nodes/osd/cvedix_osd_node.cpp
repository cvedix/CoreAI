

#include <opencv2/imgproc.hpp>
#include "cvedix_osd_node.h"

namespace cvedix_nodes {
        
    cvedix_osd_node::cvedix_osd_node(std::string node_name, std::string font):
                            cvedix_node(node_name) {
        if (!font.empty()) {
            ft2 = cv::freetype::createFreeType2();
            ft2->loadFontData(font, 0);   
        }       
        this->initialized();
    }
    
    cvedix_osd_node::~cvedix_osd_node() {
        deinitialized();
    }
    
    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_osd_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
        return meta;
    }

    void cvedix_osd_node::set_static_lines(const std::vector<osd_line_config> &lines) {
        _static_lines = lines;
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Set %zu static crosslines for always-on drawing",
            node_name.c_str(), lines.size()));
    }

    void cvedix_osd_node::set_static_zones(const std::vector<osd_zone_config> &zones) {
        _static_zones = zones;
        CVEDIX_INFO(cvedix_utils::string_format(
            "[%s] Set %zu static crowding zones for always-on drawing",
            node_name.c_str(), zones.size()));
    }

    void cvedix_osd_node::draw_static_lines(cv::Mat &canvas) {
        for (size_t i = 0; i < _static_lines.size(); ++i) {
            auto &cfg = _static_lines[i];
            auto &line = cfg.line;

            // Draw line
            cv::line(canvas,
                cv::Point(line.start.x, line.start.y),
                cv::Point(line.end.x, line.end.y),
                cfg.color, 2, cv::LINE_AA);

            // Draw endpoints
            cv::circle(canvas, cv::Point(line.start.x, line.start.y), 4, cfg.color, cv::FILLED, cv::LINE_AA);
            cv::circle(canvas, cv::Point(line.end.x, line.end.y), 4, cfg.color, cv::FILLED, cv::LINE_AA);

            // Label at midpoint
            int mid_x = (line.start.x + line.end.x) / 2;
            int mid_y = (line.start.y + line.end.y) / 2;
            std::string label = cfg.name.empty()
                ? cvedix_utils::string_format("Line %zu", i + 1)
                : cfg.name;

            int baseline = 0;
            double font_scale = 0.5;
            int font_thickness = 1;
            auto text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font_scale, font_thickness, &baseline);
            cv::Point text_org(mid_x - text_size.width / 2, mid_y - 8);

            // Background
            cv::rectangle(canvas,
                cv::Point(text_org.x - 3, text_org.y - text_size.height - 3),
                cv::Point(text_org.x + text_size.width + 3, text_org.y + 5),
                cv::Scalar(0, 0, 0), cv::FILLED);
            cv::putText(canvas, label, text_org,
                cv::FONT_HERSHEY_SIMPLEX, font_scale, cfg.color, font_thickness, cv::LINE_AA);

            // Draw direction arrows (perpendicular to line)
            double dx = line.end.x - line.start.x;
            double dy = line.end.y - line.start.y;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len > 1) {
                double perp_x = -dy / len;
                double perp_y = dx / len;
                int arrow_len = 20;

                // IN arrow
                cv::Point arrow_start(mid_x, mid_y);
                cv::Point arrow_end(mid_x + static_cast<int>(perp_x * arrow_len),
                                    mid_y + static_cast<int>(perp_y * arrow_len));
                cv::arrowedLine(canvas, arrow_start, arrow_end, cfg.color, 1, cv::LINE_AA, 0, 0.3);

                // OUT arrow
                cv::Point arrow_end2(mid_x - static_cast<int>(perp_x * arrow_len),
                                     mid_y - static_cast<int>(perp_y * arrow_len));
                cv::arrowedLine(canvas, arrow_start, arrow_end2, cfg.color, 1, cv::LINE_AA, 0, 0.3);
            }
        }
    }

    void cvedix_osd_node::draw_static_zones(cv::Mat &canvas) {
        for (size_t i = 0; i < _static_zones.size(); ++i) {
            auto &cfg = _static_zones[i];
            if (cfg.roi.size() < 3) continue;

            std::vector<cv::Point> pts;
            for (auto &p : cfg.roi) pts.emplace_back(cv::Point(p.x, p.y));

            // Zone polygon outline
            cv::polylines(canvas, pts, true, cfg.color, 2, cv::LINE_AA);

            // Semi-transparent fill
            cv::Mat overlay = canvas.clone();
            cv::fillPoly(overlay, pts, cfg.color);
            cv::addWeighted(overlay, 0.08, canvas, 0.92, 0, canvas);

            // Zone name label at centroid
            int cx = 0, cy = 0;
            for (auto &p : cfg.roi) { cx += p.x; cy += p.y; }
            cx /= static_cast<int>(cfg.roi.size());
            cy /= static_cast<int>(cfg.roi.size());

            std::string label = cfg.name.empty()
                ? cvedix_utils::string_format("Zone %zu", i + 1)
                : cfg.name;

            int baseline = 0;
            double font_scale = 0.5;
            int font_thickness = 1;
            auto text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font_scale, font_thickness, &baseline);
            cv::Point text_org(cx - text_size.width / 2, cy + text_size.height / 2);

            // Background for label
            cv::rectangle(canvas,
                cv::Point(text_org.x - 3, text_org.y - text_size.height - 3),
                cv::Point(text_org.x + text_size.width + 3, text_org.y + 5),
                cv::Scalar(0, 0, 0), cv::FILLED);
            cv::putText(canvas, label, text_org,
                cv::FONT_HERSHEY_SIMPLEX, font_scale, cfg.color, font_thickness, cv::LINE_AA);

            // Vertices
            for (auto &p : cfg.roi) {
                cv::circle(canvas, cv::Point(p.x, p.y), 3, cfg.color, cv::FILLED, cv::LINE_AA);
            }
        }
    }

    // display logic
    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_osd_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        // operations on osd_frame
        if (meta->osd_frame.empty()) {
            meta->osd_frame = meta->frame.clone();
        }

        auto& canvas = meta->osd_frame;

        // ── Static BA geometry (always drawn) ──
        draw_static_lines(canvas);
        draw_static_zones(canvas);

        // ── Detected targets ──
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
                //cv::putText(canvas, labels_to_display, cv::Point(i->x, i->y), 1, 1, cv::Scalar(255, 0, 255));
                int baseline = 0;
                auto size = cv::getTextSize(labels_to_display, 1, 1.5, 1, &baseline);
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
        return meta;
    }
}