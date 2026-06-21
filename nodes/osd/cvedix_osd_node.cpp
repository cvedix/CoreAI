
#include "cvedix_osd_node.h"
#include <cmath>

namespace cvedix_nodes {

// ═══════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════

cvedix_osd_node::cvedix_osd_node(std::string node_name, std::string font)
    : cvedix_node(node_name) {
    if (!font.empty()) {
        ft2 = cv::freetype::createFreeType2();
        ft2->loadFontData(font, 0);
    }
    // Generate pose colors
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis1(64, 200);
    std::uniform_int_distribution<> dis2(100, 255);
    std::uniform_int_distribution<> dis3(100, 255);
    for (int i = 0; i < 100; ++i) {
        _pose_colors.push_back(cv::Scalar(dis1(gen), dis2(gen), dis3(gen)));
    }
    this->initialized();
}

cvedix_osd_node::~cvedix_osd_node() {
    deinitialized();
}

// ═══════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════

void cvedix_osd_node::update_config(const unified_osd_config &config) {
    _config = config;
    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] Config updated: bbox=%d label=%d track_id=%d trail=%d dot=%d crossline=%d crowding=%d",
        node_name.c_str(), _config.show_bbox, _config.show_label,
        _config.show_track_id, _config.show_track_trail,
        _config.show_center_dot, _config.enable_ba_crossline,
        _config.enable_ba_crowding));
}

void cvedix_osd_node::set_static_lines(const std::vector<unified_static_line_config> &lines) {
    _static_lines = lines;
    CVEDIX_INFO(cvedix_utils::string_format("[%s] Set %zu static lines", node_name.c_str(), lines.size()));
}

void cvedix_osd_node::set_static_zones(const std::vector<unified_static_zone_config> &zones) {
    _static_zones = zones;
    CVEDIX_INFO(cvedix_utils::string_format("[%s] Set %zu static zones", node_name.c_str(), zones.size()));
}

void cvedix_osd_node::set_line_configs(int channel_id, const std::vector<unified_line_config> &configs) {
    _all_line_configs[channel_id] = configs;
    CVEDIX_INFO(cvedix_utils::string_format("[%s] Set %zu line configs for channel %d",
        node_name.c_str(), configs.size(), channel_id));
}

void cvedix_osd_node::set_line_color(int channel_id, int line_index, const cv::Scalar &color) {
    if (_all_line_configs.count(channel_id) == 0) return;
    auto &configs = _all_line_configs[channel_id];
    if (line_index >= 0 && line_index < static_cast<int>(configs.size())) {
        configs[line_index].color = color;
    }
}

void cvedix_osd_node::set_seg_config(const std::vector<std::string> &classes,
                                              const std::vector<cv::Vec3b> &colors) {
    _seg_classes = classes;
    _seg_colors = colors;
    CVEDIX_INFO(cvedix_utils::string_format("[%s] Set %zu seg classes, %zu colors",
        node_name.c_str(), classes.size(), colors.size()));
}

// ═══════════════════════════════════════════
// Main handler — auto-detect and render
// ═══════════════════════════════════════════

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_osd_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    if (meta->osd_frame.empty()) {
        meta->osd_frame = meta->frame;
    }
    auto &canvas = meta->osd_frame;

    // 1. Static geometry (always-on)
    if (_config.show_static_lines) render_static_lines(canvas);
    if (_config.show_static_zones) render_static_zones(canvas);

    // 2. BA layers (auto-detect from ba_results)
    if (!meta->ba_results.empty()) {
        if (_config.enable_ba_crossline)  render_ba_crossline(canvas, meta);
        if (_config.enable_ba_crowding)   render_ba_crowding(canvas, meta);
        if (_config.enable_ba_jam)        render_ba_jam(canvas, meta);
        if (_config.enable_ba_stop)       render_ba_stop(canvas, meta);
        if (_config.enable_ba_enter_exit) render_ba_enter_exit(canvas, meta);
    }

    // 3. Target rendering
    render_targets(canvas, meta);

    // 4. Domain layers (auto-detect from metadata)
    if (_config.enable_face && !meta->face_targets.empty()) render_face(canvas, meta);
    if (_config.enable_pose && !meta->pose_targets.empty()) render_pose(canvas, meta);

    // 5. Instance mask overlay (per target)
    if (_config.enable_instance_mask) render_instance_mask(canvas, meta);

    // 6. Additional domain overlays (auto-detect from metadata)
    if (_config.enable_text_region && !meta->text_targets.empty()) render_text_region(canvas, meta);
    if (_config.enable_expr && !meta->text_targets.empty()) render_expr(canvas, meta);
    if (_config.enable_lane && !meta->mask.empty()) render_lane(canvas, meta);
    if (_config.enable_plate) render_plate(canvas, meta);
    if (_config.enable_seg && !meta->mask.empty()) render_seg(canvas, meta);
    if (_config.enable_mllm && !meta->description.empty()) render_mllm(canvas, meta);
    if (_config.enable_sub_thumbnails) render_sub_thumbnails(canvas, meta);

    return meta;
}

std::shared_ptr<cvedix_objects::cvedix_meta>
cvedix_osd_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
    return meta;
}

// ═══════════════════════════════════════════
// Target Rendering Layer
// ═══════════════════════════════════════════

void cvedix_osd_node::render_targets(cv::Mat &canvas,
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    // Collect alerted IDs for coloring (persistent)
    for (auto &ba : meta->ba_results) {
        if (ba->type == cvedix_objects::cvedix_ba_type::CROSSLINE ||
            ba->type == cvedix_objects::cvedix_ba_type::RED_LIGHT ||
            ba->type == cvedix_objects::cvedix_ba_type::STOP_LINE) {
            for (auto &tid : ba->involve_target_ids_in_frame) {
                _all_crossed_track_ids.insert(tid);
            }
        }
    }

    for (auto &i : meta->targets) {
        // Skip untracked targets if only showing track_id
        if (_config.show_track_id && !_config.show_bbox && !_config.show_label && i->track_id < 0)
            continue;

        bool has_crossed = (i->track_id != -1 && _all_crossed_track_ids.count(i->track_id) > 0);

        // Determine colors
        cv::Scalar dot_color = has_crossed ? _config.alert_color : _config.dot_color;
        cv::Scalar trail_color = has_crossed ? _config.alert_color : _config.trail_color;
        cv::Scalar bbox_color = has_crossed ? _config.alert_color : _config.bbox_color;
        cv::Scalar text_color = has_crossed ? _config.alert_color : _config.dot_color;

        // Blur target region if label matches blur_labels list
        if (!_config.blur_labels.empty()) {
            for (const auto &bl : _config.blur_labels) {
                if (i->primary_label == bl) {
                    int bx = std::max(0, i->x);
                    int by = std::max(0, i->y);
                    int bw = std::min(i->width,  canvas.cols - bx);
                    int bh = std::min(i->height, canvas.rows - by);
                    if (bw > 5 && bh > 5) {
                        cv::Rect roi(bx, by, bw, bh);
                        int ks = _config.face_blur_kernel_size | 1;
                        cv::GaussianBlur(canvas(roi), canvas(roi), cv::Size(ks, ks), 0);
                    }
                    break;
                }
            }
        }

        // Track trail
        if (_config.show_track_trail && i->tracks.size() >= 2) {
            for (size_t n = 0; n < (i->tracks.size() - 1); n++) {
                auto p1 = i->tracks[n].track_point();
                auto p2 = i->tracks[n + 1].track_point();
                cv::line(canvas, cv::Point(p1.x, p1.y), cv::Point(p2.x, p2.y),
                         trail_color, 1, cv::LINE_AA);
            }
        }

        // Bounding box — corner-bracket style (4 corners)
        if (_config.show_bbox) {
            cv::Scalar box_color = bbox_color;
            int th = _config.bbox_thickness;
            int x1 = i->x, y1 = i->y;
            int x2 = i->x + i->width, y2 = i->y + i->height;
            int corner_len = std::max(10, std::min(i->width, i->height) / 5);

            // Top-left corner
            cv::line(canvas, cv::Point(x1, y1), cv::Point(x1 + corner_len, y1), box_color, th, cv::LINE_AA);
            cv::line(canvas, cv::Point(x1, y1), cv::Point(x1, y1 + corner_len), box_color, th, cv::LINE_AA);
            // Top-right corner
            cv::line(canvas, cv::Point(x2, y1), cv::Point(x2 - corner_len, y1), box_color, th, cv::LINE_AA);
            cv::line(canvas, cv::Point(x2, y1), cv::Point(x2, y1 + corner_len), box_color, th, cv::LINE_AA);
            // Bottom-left corner
            cv::line(canvas, cv::Point(x1, y2), cv::Point(x1 + corner_len, y2), box_color, th, cv::LINE_AA);
            cv::line(canvas, cv::Point(x1, y2), cv::Point(x1, y2 - corner_len), box_color, th, cv::LINE_AA);
            // Bottom-right corner
            cv::line(canvas, cv::Point(x2, y2), cv::Point(x2 - corner_len, y2), box_color, th, cv::LINE_AA);
            cv::line(canvas, cv::Point(x2, y2), cv::Point(x2, y2 - corner_len), box_color, th, cv::LINE_AA);
        }

        // Center dot
        if (_config.show_center_dot) {
            int cx = i->x + i->width / 2;
            int cy = i->y + i->height / 2;
            cv::circle(canvas, cv::Point(cx, cy), _config.dot_radius,
                       dot_color, cv::FILLED, cv::LINE_AA);
            cv::circle(canvas, cv::Point(cx, cy), _config.dot_radius,
                       cv::Scalar(0, 0, 0), 1, cv::LINE_AA);

            // Track ID next to dot
            if (_config.show_track_id && i->track_id >= 0) {
                cv::putText(canvas, std::to_string(i->track_id),
                            cv::Point(cx + _config.dot_radius + 3, cy + 4),
                            cv::FONT_HERSHEY_SIMPLEX, _config.label_font_scale,
                            text_color, 1, cv::LINE_AA);
            }
        }

        // Class label (below bbox or at top-left of target)
        if (_config.show_label) {
            std::string label = i->primary_label;
            if (_config.show_track_id && i->track_id >= 0) {
                label = "#" + std::to_string(i->track_id) + " " + label;
            }
            for (auto &sec : i->secondary_labels) {
                label += "|" + sec;
            }

            int baseline = 0;
            auto text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX,
                                              _config.label_font_scale, 1, &baseline);
            int text_x = i->x;
            int text_y = i->y + i->height + text_size.height + 4;

            cv::Scalar bg = has_crossed ? cv::Scalar(0, 0, 100) : cv::Scalar(0, 0, 0);
            cv::rectangle(canvas,
                          cv::Point(text_x - 1, i->y + i->height + 1),
                          cv::Point(text_x + text_size.width + 4, text_y + 3),
                          bg, cv::FILLED);
            cv::putText(canvas, label, cv::Point(text_x + 2, text_y),
                        cv::FONT_HERSHEY_SIMPLEX, _config.label_font_scale,
                        text_color, 1, cv::LINE_AA);
        }

        // Sub-targets
        if (_config.show_sub_targets) {
            for (auto &sub : i->sub_targets) {
                cv::rectangle(canvas, cv::Rect(sub->x, sub->y, sub->width, sub->height),
                              cv::Scalar(255), 1);
                cv::putText(canvas, sub->label, cv::Point(sub->x, sub->y),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
            }
        }
    }
}

// ═══════════════════════════════════════════
// Static Geometry Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::render_static_lines(cv::Mat &canvas) {
    for (size_t i = 0; i < _static_lines.size(); ++i) {
        auto &cfg = _static_lines[i];
        auto &line = cfg.line;

        cv::line(canvas,
                 cv::Point(line.start.x, line.start.y),
                 cv::Point(line.end.x, line.end.y),
                 cfg.color, 3, cv::LINE_AA);

        // Label at midpoint
        int mid_x = (line.start.x + line.end.x) / 2;
        int mid_y = (line.start.y + line.end.y) / 2;
        std::string label = cfg.name.empty()
            ? cvedix_utils::string_format("Line %zu", i + 1) : cfg.name;

        int baseline = 0;
        double font_scale = 0.5;
        auto text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font_scale, 1, &baseline);
        cv::Point text_org(mid_x - text_size.width / 2, mid_y - 8);
        cv::rectangle(canvas,
            cv::Point(text_org.x - 3, text_org.y - text_size.height - 3),
            cv::Point(text_org.x + text_size.width + 3, text_org.y + 5),
            cv::Scalar(0, 0, 0), cv::FILLED);
        cv::putText(canvas, label, text_org,
            cv::FONT_HERSHEY_SIMPLEX, font_scale, cfg.color, 1, cv::LINE_AA);

        // Direction arrows
        double dx = line.end.x - line.start.x;
        double dy = line.end.y - line.start.y;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len > 1) {
            double perp_x = -dy / len;
            double perp_y = dx / len;
            int arrow_len = 20;

            cv::Point arrow_start(mid_x, mid_y);
            cv::Point arrow_end1(mid_x + static_cast<int>(perp_x * arrow_len),
                                 mid_y + static_cast<int>(perp_y * arrow_len));
            cv::arrowedLine(canvas, arrow_start, arrow_end1, cfg.color, 1, cv::LINE_AA, 0, 0.3);

            cv::Point arrow_end2(mid_x - static_cast<int>(perp_x * arrow_len),
                                 mid_y - static_cast<int>(perp_y * arrow_len));
            cv::arrowedLine(canvas, arrow_start, arrow_end2, cfg.color, 1, cv::LINE_AA, 0, 0.3);
        }
    }
}

void cvedix_osd_node::render_static_zones(cv::Mat &canvas) {
    for (size_t i = 0; i < _static_zones.size(); ++i) {
        auto &cfg = _static_zones[i];
        if (cfg.roi.size() < 3) continue;

        std::vector<cv::Point> pts;
        for (auto &p : cfg.roi) pts.emplace_back(cv::Point(p.x, p.y));

        cv::polylines(canvas, pts, true, cfg.color, 2, cv::LINE_AA);

        // Semi-transparent fill
        cv::Mat overlay = canvas.clone();
        cv::fillPoly(overlay, pts, cfg.color);
        cv::addWeighted(overlay, 0.08, canvas, 0.92, 0, canvas);

        // Zone name at centroid
        int cx = 0, cy = 0;
        for (auto &p : cfg.roi) { cx += p.x; cy += p.y; }
        cx /= static_cast<int>(cfg.roi.size());
        cy /= static_cast<int>(cfg.roi.size());

        std::string label = cfg.name.empty()
            ? cvedix_utils::string_format("Zone %zu", i + 1) : cfg.name;

        int baseline = 0;
        auto text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::Point text_org(cx - text_size.width / 2, cy + text_size.height / 2);

        cv::rectangle(canvas,
            cv::Point(text_org.x - 3, text_org.y - text_size.height - 3),
            cv::Point(text_org.x + text_size.width + 3, text_org.y + 5),
            cv::Scalar(0, 0, 0), cv::FILLED);
        cv::putText(canvas, label, text_org,
            cv::FONT_HERSHEY_SIMPLEX, 0.5, cfg.color, 1, cv::LINE_AA);

        for (auto &p : cfg.roi) {
            cv::circle(canvas, cv::Point(p.x, p.y), 3, cfg.color, cv::FILLED, cv::LINE_AA);
        }
    }
}

// ═══════════════════════════════════════════
// BA Crossline Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::draw_direction_arrow(
    cv::Mat &canvas, const cvedix_objects::cvedix_line &line,
    const cv::Scalar &color, cvedix_objects::cvedix_ba_direct_type direction) {

    int mid_x = (line.start.x + line.end.x) / 2;
    int mid_y = (line.start.y + line.end.y) / 2;

    double dx = line.end.x - line.start.x;
    double dy = line.end.y - line.start.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1) return;

    double perp_x = -dy / len;
    double perp_y = dx / len;
    int arrow_len = 25;

    if (direction == cvedix_objects::cvedix_ba_direct_type::IN ||
        direction == cvedix_objects::cvedix_ba_direct_type::BOTH) {
        cv::Point start(mid_x, mid_y);
        cv::Point end(mid_x + static_cast<int>(perp_x * arrow_len),
                      mid_y + static_cast<int>(perp_y * arrow_len));
        cv::arrowedLine(canvas, start, end, color, 2, cv::LINE_AA, 0, 0.3);
        cv::putText(canvas, "IN", cv::Point(end.x + 5, end.y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1);
    }

    if (direction == cvedix_objects::cvedix_ba_direct_type::OUT ||
        direction == cvedix_objects::cvedix_ba_direct_type::BOTH) {
        cv::Point start(mid_x, mid_y);
        cv::Point end(mid_x - static_cast<int>(perp_x * arrow_len),
                      mid_y - static_cast<int>(perp_y * arrow_len));
        cv::arrowedLine(canvas, start, end, color, 2, cv::LINE_AA, 0, 0.3);
        cv::putText(canvas, "OUT", cv::Point(end.x + 5, end.y),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1);
    }
}

void cvedix_osd_node::render_ba_crossline(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    // Use channel 0 line configs by default
    auto &line_configs = _all_line_configs[0];
    auto &total_crossline = _all_total_crossline[0];

    // Collect crossed track IDs
    for (auto &ba : meta->ba_results) {
        if (ba->type == cvedix_objects::cvedix_ba_type::CROSSLINE) {
            for (auto &tid : ba->involve_target_ids_in_frame) {
                _all_crossed_track_ids.insert(tid);
            }
        }
    }

    // Update line configs from BA results
    for (auto &ba : meta->ba_results) {
        if (ba->type == cvedix_objects::cvedix_ba_type::CROSSLINE) {
            if (ba->involve_region_in_frame.size() == 2) {
                cvedix_objects::cvedix_line line(ba->involve_region_in_frame[0],
                                                 ba->involve_region_in_frame[1]);
                int line_index = 0;
                std::string label = ba->ba_label;
                size_t pos = label.find("cross line ");
                if (pos != std::string::npos) {
                    try { line_index = std::stoi(label.substr(pos + 11)); }
                    catch (...) { line_index = 0; }
                }

                while (line_configs.size() <= static_cast<size_t>(line_index)) {
                    size_t idx = line_configs.size();
                    cv::Scalar color = _default_colors[idx % _default_colors.size()];
                    line_configs.push_back(unified_line_config(cvedix_objects::cvedix_line(), color));
                }

                line_configs[line_index].line = line;
                line_configs[line_index].crossing_count++;
                total_crossline++;
            }
        }
    }

    // Draw all lines
    for (size_t i = 0; i < line_configs.size(); i++) {
        auto &cfg = line_configs[i];
        if (cfg.line.start.x == 0 && cfg.line.start.y == 0 &&
            cfg.line.end.x == 0 && cfg.line.end.y == 0) continue;

        cv::line(canvas,
                 cv::Point(cfg.line.start.x, cfg.line.start.y),
                 cv::Point(cfg.line.end.x, cfg.line.end.y),
                 cfg.color, 3, cv::LINE_AA);

        draw_direction_arrow(canvas, cfg.line, cfg.color, cfg.direction);

        int mid_x = (cfg.line.start.x + cfg.line.end.x) / 2;
        int mid_y = (cfg.line.start.y + cfg.line.end.y) / 2;
        std::string line_label = cfg.name.empty()
            ? cvedix_utils::string_format("Line %zu", i) : cfg.name;
        cv::putText(canvas, line_label, cv::Point(mid_x - 30, mid_y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cfg.color, 2);
    }
}

// ═══════════════════════════════════════════
// BA Crowding Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::render_ba_crowding(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    for (auto &ba : meta->ba_results) {
        if (ba->ba_label != "crowding") continue;

        // Draw ROI polygon
        auto &region = ba->involve_region_in_frame;
        std::vector<cv::Point> pts;
        for (auto &p : region) pts.emplace_back(cv::Point(p.x, p.y));
        if (!pts.empty()) {
            cv::polylines(canvas, pts, true, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
        }

        // Highlight involved targets
        auto targets = meta->get_targets_by_ids(ba->involve_target_ids_in_frame);
        for (auto &t : targets) {
            cv::rectangle(canvas, cv::Rect(t->x, t->y, t->width, t->height),
                          cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
        }




    }
}

// ═══════════════════════════════════════════
// BA Jam Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::render_ba_jam(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    int ch = meta->channel_index;
    auto &region = _jam_regions[ch];
    auto &jam_active = _jam_active[ch];
    auto &involve_ids = _jam_involve_ids[ch];

    for (auto &ba : meta->ba_results) {
        if (ba->type == cvedix_objects::cvedix_ba_type::JAM) {
            region = ba->involve_region_in_frame;
            involve_ids = ba->involve_target_ids_in_frame;
            jam_active = true;
        }
        if (ba->type == cvedix_objects::cvedix_ba_type::UNJAM) {
            region = ba->involve_region_in_frame;
            jam_active = false;
        }
    }

    if (jam_active && !region.empty()) {
        std::vector<cv::Point> pts;
        for (auto &p : region) pts.push_back(cv::Point(p.x, p.y));
        cv::polylines(canvas, pts, true, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

        auto targets = meta->get_targets_by_ids(involve_ids);
        for (auto &t : targets) {
            cv::rectangle(canvas, cv::Rect(t->x, t->y, t->width, t->height),
                          cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
        }
    }
}

// ═══════════════════════════════════════════
// BA Stop Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::render_ba_stop(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    int ch = meta->channel_index;
    auto &region = _stop_regions[ch];
    auto &active_ids = _stop_active_ids[ch];

    for (auto &ba : meta->ba_results) {
        if (ba->type == cvedix_objects::cvedix_ba_type::STOP) {
            region = ba->involve_region_in_frame;
            active_ids.push_back(ba->involve_target_ids_in_frame[0]);
        }
        if (ba->type == cvedix_objects::cvedix_ba_type::UNSTOP) {
            region = ba->involve_region_in_frame;
            for (auto it = active_ids.begin(); it != active_ids.end();) {
                if (*it == ba->involve_target_ids_in_frame[0]) {
                    it = active_ids.erase(it);
                    break;
                }
                ++it;
            }
        }
    }

    if (!region.empty()) {
        std::vector<cv::Point> pts;
        for (auto &p : region) pts.push_back(cv::Point(p.x, p.y));
        cv::polylines(canvas, pts, true, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    }

    auto stop_targets = meta->get_targets_by_ids(active_ids);
    for (auto &t : stop_targets) {
        cv::rectangle(canvas, cv::Rect(t->x, t->y, t->width, t->height),
                      cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    }
}

// ═══════════════════════════════════════════
// BA Enter/Exit Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::render_ba_enter_exit(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    int ch = meta->channel_index;
    static const cv::Scalar enter_color(0, 220, 0);
    static const cv::Scalar exit_color(0, 0, 220);

    // Decrement TTLs
    for (auto it = _ee_polys[ch].begin(); it != _ee_polys[ch].end();) {
        if (--it->ttl <= 0) it = _ee_polys[ch].erase(it); else ++it;
    }
    for (auto it = _ee_alerts[ch].begin(); it != _ee_alerts[ch].end();) {
        if (--it->ttl <= 0) it = _ee_alerts[ch].erase(it); else ++it;
    }

    // Process BA results
    for (auto &ba : meta->ba_results) {
        if (ba->type == cvedix_objects::cvedix_ba_type::AREA_ENTER ||
            ba->type == cvedix_objects::cvedix_ba_type::AREA_EXIT) {
            cv::Scalar color = (ba->type == cvedix_objects::cvedix_ba_type::AREA_ENTER)
                                   ? enter_color : exit_color;

            _active_poly ap;
            ap.color = color;
            ap.ttl = _ee_ttl_frames;
            ap.poly = ba->involve_region_in_frame;
            _ee_polys[ch].push_back(ap);

            for (auto tid : ba->involve_target_ids_in_frame) {
                std::string type_label = (ba->type == cvedix_objects::cvedix_ba_type::AREA_ENTER)
                                             ? "Enter" : "Exit";
                _active_alert al;
                al.color = color;
                al.ttl = _ee_ttl_frames;
                al.text = cvedix_utils::string_format("ID %d - %s", tid, type_label.c_str());
                _ee_alerts[ch].push_back(al);
            }
        }
    }

    // Draw active polygons
    for (auto &ap : _ee_polys[ch]) {
        std::vector<cv::Point> pts;
        for (auto &p : ap.poly) pts.push_back(cv::Point(p.x, p.y));
        if (pts.size() >= 2) {
            cv::polylines(canvas, pts, true, ap.color, 3, cv::LINE_AA);
        }
    }


}

// ═══════════════════════════════════════════
// Face Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::render_face(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    for (auto &i : meta->face_targets) {
        int x = std::max(0, std::min(i->x, canvas.cols - 1));
        int y = std::max(0, std::min(i->y, canvas.rows - 1));
        int w = std::max(1, std::min(i->width, canvas.cols - x));
        int h = std::max(1, std::min(i->height, canvas.rows - y));

        if (w < 10 || h < 10) continue;

        // Apply Gaussian blur to face region for privacy protection
        if (_config.enable_face_blur) {
            cv::Rect face_roi(x, y, w, h);
            int ks = _config.face_blur_kernel_size | 1; // ensure odd
            cv::GaussianBlur(canvas(face_roi), canvas(face_roi), cv::Size(ks, ks), 0);
        }

        // ── Determine bbox color based on liveness status ──
        cv::Scalar bbox_color = _config.bbox_color;
        if (i->liveness_status == 0)       bbox_color = cv::Scalar(0, 200, 0);     // REAL → green
        else if (i->liveness_status == 1)  bbox_color = cv::Scalar(0, 0, 255);     // SPOOF → red
        else if (i->liveness_status == 2)  bbox_color = cv::Scalar(0, 200, 255);   // FUZZY → yellow

        cv::rectangle(canvas, cv::Rect(x, y, w, h), bbox_color, _config.bbox_thickness);

        // ── Build info labels ──
        int label_y = y - 5;
        double font_scale = _config.label_font_scale;
        int font = cv::FONT_HERSHEY_SIMPLEX;
        int thickness = 1;

        // Line 1: Identity or "Unknown" + track ID
        std::string line1 = "";
        if (!i->identify.empty()) {
            line1 = i->identify;
            if (i->identify_score > 0)
                line1 += " " + cvedix_utils::string_format("%.0f%%", i->identify_score * 100);
        }
        if (i->track_id != -1) {
            line1 = (line1.empty() ? "" : line1 + " ") + "#" + std::to_string(i->track_id);
        }

        // Line 2: Gender + Age + Mask
        std::string line2 = "";
        if (!i->gender_str.empty()) line2 += i->gender_str;
        if (i->age >= 0) line2 += (line2.empty() ? "" : ", ") + std::to_string(i->age) + "y";
        if (i->wearing_mask) line2 += (line2.empty() ? "" : " ") + std::string("[Mask]");

        // Line 3: Eye state + Liveness
        std::string line3 = "";
        auto eye_str = [](int s) -> std::string {
            switch(s) {
                case 0: return "Closed";
                case 1: return "Open";
                case 2: return "Random";
                default: return "";
            }
        };
        std::string left_e = eye_str(i->left_eye_state);
        std::string right_e = eye_str(i->right_eye_state);
        if (!left_e.empty() || !right_e.empty()) {
            line3 += "Eyes:";
            if (!left_e.empty()) line3 += "L=" + left_e;
            if (!right_e.empty()) line3 += (left_e.empty() ? "" : ",") + std::string("R=") + right_e;
        }
        if (i->liveness_status >= 0) {
            std::string lv;
            switch(i->liveness_status) {
                case 0: lv = "Real"; break;
                case 1: lv = "SPOOF!"; break;
                case 2: lv = "Fuzzy"; break;
                default: lv = "?"; break;
            }
            line3 += (line3.empty() ? "" : " | ") + lv;
        }

        // Line 4: Head pose
        std::string line4 = "";
        if (i->pose_valid) {
            line4 = cvedix_utils::string_format("Y:%.0f P:%.0f R:%.0f", i->yaw, i->pitch, i->roll);
        }

        // ── Draw labels with dark background ──
        auto draw_label = [&](const std::string& text, int& ly, cv::Scalar color) {
            if (text.empty()) return;
            int baseline = 0;
            auto sz = cv::getTextSize(text, font, font_scale, thickness, &baseline);
            cv::rectangle(canvas,
                cv::Point(x, ly - sz.height - 4),
                cv::Point(x + sz.width + 6, ly + 2),
                cv::Scalar(0, 0, 0), cv::FILLED);
            cv::putText(canvas, text, cv::Point(x + 3, ly - 1), font, font_scale, color, thickness, cv::LINE_AA);
            ly -= (sz.height + 6);
        };

        // Draw from bottom to top above the bbox
        if (!line4.empty()) draw_label(line4, label_y, cv::Scalar(200, 200, 200));
        if (!line3.empty()) {
            cv::Scalar c3 = (i->liveness_status == 1) ? cv::Scalar(100, 100, 255) : cv::Scalar(200, 255, 200);
            draw_label(line3, label_y, c3);
        }
        if (!line2.empty()) draw_label(line2, label_y, cv::Scalar(255, 220, 150));
        if (!line1.empty()) draw_label(line1, label_y, cv::Scalar(255, 255, 255));

        // ── 5-point keypoints ──
        if (i->key_points.size() >= 5) {
            static const cv::Scalar kp_colors[] = {
                cv::Scalar(255, 0, 0),   cv::Scalar(0, 0, 255),
                cv::Scalar(0, 255, 0),   cv::Scalar(255, 0, 255),
                cv::Scalar(0, 255, 255)
            };
            for (size_t kp = 0; kp < 5 && kp < i->key_points.size(); ++kp) {
                int kx = std::max(0, std::min(i->key_points[kp].first, canvas.cols - 1));
                int ky = std::max(0, std::min(i->key_points[kp].second, canvas.rows - 1));
                cv::circle(canvas, cv::Point(kx, ky), 3, kp_colors[kp], 2);
            }
        }

        // ── Head pose direction arrow ──
        if (i->pose_valid) {
            int cx = x + w / 2;
            int cy = y + h / 2;
            int arrow_len = std::min(w, h) / 3;
            double yaw_rad = i->yaw * CV_PI / 180.0;
            double pitch_rad = i->pitch * CV_PI / 180.0;
            int dx = static_cast<int>(arrow_len * std::sin(yaw_rad));
            int dy = static_cast<int>(-arrow_len * std::sin(pitch_rad));
            cv::arrowedLine(canvas, cv::Point(cx, cy), cv::Point(cx + dx, cy + dy),
                           cv::Scalar(0, 255, 255), 2, cv::LINE_AA, 0, 0.3);
        }
    }
}

// ═══════════════════════════════════════════
// Pose Rendering
// ═══════════════════════════════════════════

void cvedix_osd_node::render_pose(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    for (size_t i = 0; i < meta->pose_targets.size(); i++) {
        auto &pose_target = meta->pose_targets[i];
        auto it = _pose_pairs.find(pose_target->type);
        if (it == _pose_pairs.end()) continue;

        auto &pairs = it->second;
        for (size_t j = 0; j < pairs.size(); j++) {
            auto &a = pose_target->key_points[pairs[j].first];
            auto &b = pose_target->key_points[pairs[j].second];
            if (a.x < 0 || a.y < 0 || b.x < 0 || b.y < 0) continue;

            cv::Scalar color = _pose_colors[j % _pose_colors.size()];
            cv::line(canvas, cv::Point(a.x, a.y), cv::Point(b.x, b.y), color, 2, cv::LINE_AA);
            cv::circle(canvas, cv::Point(a.x, a.y), 3, color, -1, cv::LINE_AA);
            cv::circle(canvas, cv::Point(b.x, b.y), 3, color, -1, cv::LINE_AA);
        }
    }
}

// ═══════════════════════════════════════════
// Instance Mask Rendering (from osd_v3)
// ═══════════════════════════════════════════

void cvedix_osd_node::render_instance_mask(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    for (auto &i : meta->targets) {
        if (i->mask.empty()) continue;

        cv::resize(i->mask, i->mask, cv::Size(i->width, i->height));
        cv::Mat mask = (i->mask > _config.mask_threshold);
        cv::Mat coloredRoi = (0.3 * cv::Scalar(255, 255, 0) + 0.7 * canvas(cv::Rect(i->x, i->y, i->width, i->height)));
        coloredRoi.convertTo(coloredRoi, CV_8UC3);

        std::vector<cv::Mat> contours;
        cv::Mat hierarchy;
        mask.convertTo(mask, CV_8U);
        cv::findContours(mask, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
        cv::drawContours(coloredRoi, contours, -1, cv::Scalar(255, 255, 0), 5, cv::LINE_8, hierarchy, 100);
        coloredRoi.copyTo(canvas(cv::Rect(i->x, i->y, i->width, i->height)), mask);
    }
}

// ═══════════════════════════════════════════
// Text Region Rendering (from text_osd — fixed canvas overlay)
// ═══════════════════════════════════════════

void cvedix_osd_node::render_text_region(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    for (auto &text : meta->text_targets) {
        if (text->region_vertexes.size() < 4) continue;

        cv::Point rook_points[4];
        for (int m = 0; m < (int)text->region_vertexes.size() && m < 4; m++) {
            rook_points[m] = cv::Point(text->region_vertexes[m].first, text->region_vertexes[m].second);
        }

        const cv::Point *ppt[1] = {rook_points};
        int npt[] = {4};
        cv::polylines(canvas, ppt, npt, 1, 1, CV_RGB(0, 255, 0), 2, cv::LINE_AA, 0);

        // Overlay detected text near the vertex (instead of double-height canvas)
        if (ft2 != nullptr && !text->text.empty()) {
            ft2->putText(canvas, text->text, rook_points[3], 20, cv::Scalar(255, 0, 0),
                         cv::FILLED, cv::LINE_AA, true);
        }
    }
}

// ═══════════════════════════════════════════
// Expression Flag Rendering (from expr_osd)
// ═══════════════════════════════════════════

void cvedix_osd_node::render_expr(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    for (auto &i : meta->text_targets) {
        if (i->region_vertexes.size() < 4) continue;

        cv::Point rook_points[4];
        for (int m = 0; m < (int)i->region_vertexes.size() && m < 4; m++) {
            rook_points[m] = cv::Point(i->region_vertexes[m].first, i->region_vertexes[m].second);
        }

        const cv::Point *ppt[1] = {rook_points};
        int npt[] = {4};

        if (i->flags.find("yes") != std::string::npos) {
            cv::polylines(canvas, ppt, npt, 1, 1, CV_RGB(0, 255, 0), 2, cv::LINE_AA, 0);
            if (ft2 != nullptr)
                ft2->putText(canvas, "\u221A", rook_points[1], 30, CV_RGB(0, 255, 0), cv::FILLED, cv::LINE_AA, true);
        }
        else if (i->flags.find("no") != std::string::npos) {
            auto right_value = cvedix_utils::string_split(i->flags, '_')[1];
            cv::polylines(canvas, ppt, npt, 1, 1, CV_RGB(255, 0, 0), 2, cv::LINE_AA, 0);
            if (ft2 != nullptr)
                ft2->putText(canvas, "\u00d7(" + right_value + ")", rook_points[1], 30, CV_RGB(255, 0, 0), cv::FILLED, cv::LINE_AA, true);
        }
        else if (i->flags == "invalid") {
            cv::polylines(canvas, ppt, npt, 1, 1, CV_RGB(255, 165, 0), 2, cv::LINE_AA, 0);
            if (ft2 != nullptr)
                ft2->putText(canvas, "invalid", rook_points[1], 30, CV_RGB(255, 165, 0), cv::FILLED, cv::LINE_AA, true);
        }
    }
}

// ═══════════════════════════════════════════
// Lane Mask Rendering (from lane_osd)
// ═══════════════════════════════════════════

void cvedix_osd_node::render_lane(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    if (meta->mask.empty()) return;

    cv::Mat mask(meta->mask.size[2], meta->mask.size[3], CV_32FC1, meta->mask.data);
    cv::Mat mask_big;
    cv::resize(mask, mask_big, canvas.size());
    cv::threshold(mask_big, mask_big, 0.5, 1, cv::THRESH_BINARY);

    auto kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(11, 11));
    cv::erode(mask_big, mask_big, kernel);
    mask_big.convertTo(mask_big, CV_8U, 255);

    for (int y = 0; y < canvas.rows; ++y) {
        for (int x = 0; x < canvas.cols; ++x) {
            canvas.at<cv::Vec3b>(y, x)[2] = mask_big.at<uchar>(y, x);
            canvas.at<cv::Vec3b>(y, x)[1] = cv::saturate_cast<uchar>(
                canvas.at<cv::Vec3b>(y, x)[1] * 0.8 + mask_big.at<uchar>(y, x) * 0.2);
            canvas.at<cv::Vec3b>(y, x)[0] = cv::saturate_cast<uchar>(
                canvas.at<cv::Vec3b>(y, x)[0] * 0.8 + mask_big.at<uchar>(y, x) * 0.2);
        }
    }
}

// ═══════════════════════════════════════════
// Plate Rendering (from plate_osd — fixed canvas overlay)
// ═══════════════════════════════════════════

void cvedix_osd_node::render_plate(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    for (auto &i : meta->targets) {
        auto color_and_text = cvedix_utils::string_split(i->primary_label, '_');
        if (color_and_text.size() != 2) continue;

        auto &color = color_and_text[0];
        auto &text = color_and_text[1];

        auto text_it = _plate_text_colors.find(color);
        auto draw_it = _plate_draw_colors.find(color);
        if (text_it == _plate_text_colors.end() || draw_it == _plate_draw_colors.end()) continue;

        auto text_2_display = text_it->second + " " + text;
        if (i->track_id != -1) {
            text_2_display = "#" + std::to_string(i->track_id) + " " + text_2_display;
        }

        cv::rectangle(canvas, cv::Rect(i->x, i->y, i->width, i->height), draw_it->second, 2);
        if (ft2 != nullptr) {
            ft2->putText(canvas, text_2_display, cv::Point(i->x, i->y), 20,
                         draw_it->second, cv::FILLED, cv::LINE_AA, true);
        }

        // Collect history thumbnails (overlay at bottom of frame)
        if (i->width > 0 && i->height > 0) {
            auto plate = meta->frame(cv::Rect(i->x, i->y, i->width, i->height));
            cv::Mat resized;
            cv::resize(plate, resized, cv::Size(_plate_his_height,
                int((float(_plate_his_height) / plate.cols) * plate.rows)));
            _plate_history.push_back(resized);
        }
    }

    // Draw history overlay at bottom of canvas
    if (!_plate_history.empty()) {
        // Trim if overflows
        auto width_need = _plate_history.size() * (_plate_his_height + _plate_his_gap) + _plate_his_gap;
        while (width_need >= (size_t)canvas.cols) {
            _plate_history.erase(_plate_history.begin());
            width_need = _plate_history.size() * (_plate_his_height + _plate_his_gap) + _plate_his_gap;
        }

        // Semi-transparent background strip
        int strip_h = _plate_his_height + _plate_his_gap * 2;
        int strip_y = canvas.rows - strip_h;
        if (strip_y > 0) {
            cv::Mat overlay = canvas.clone();
            cv::rectangle(overlay, cv::Rect(0, strip_y, canvas.cols, strip_h),
                          cv::Scalar(240, 200, 220), cv::FILLED);
            cv::addWeighted(overlay, 0.6, canvas, 0.4, 0, canvas);

            for (size_t idx = 0; idx < _plate_history.size(); idx++) {
                auto &p = _plate_history[idx];
                int px = (_plate_his_gap + _plate_his_height) * (int)idx + _plate_his_gap;
                int py = canvas.rows - _plate_his_height / 2 - p.rows / 2;
                if (px + p.cols <= canvas.cols && py >= 0 && py + p.rows <= canvas.rows) {
                    auto roi = canvas(cv::Rect(px, py, p.cols, p.rows));
                    p.copyTo(roi);
                }
            }
        }
    }
}

// ═══════════════════════════════════════════
// Segmentation Rendering (from seg_osd — fixed canvas overlay)
// ═══════════════════════════════════════════

void cvedix_osd_node::colorize_segmentation(const cv::Mat &score, cv::Mat &segm) {
    const int rows = score.size[2];
    const int cols = score.size[3];
    const int chns = score.size[1];

    if (_seg_colors.empty()) {
        _seg_colors.push_back(cv::Vec3b());
        for (int i = 1; i < chns; ++i) {
            cv::Vec3b color;
            for (int j = 0; j < 3; ++j)
                color[j] = (_seg_colors[i - 1][j] + rand() % 256) / 2;
            _seg_colors.push_back(color);
        }
    }

    cv::Mat maxCl = cv::Mat::zeros(rows, cols, CV_8UC1);
    cv::Mat maxVal(rows, cols, CV_32FC1, score.data);
    for (int ch = 1; ch < chns; ch++) {
        for (int row = 0; row < rows; row++) {
            const float *ptrScore = score.ptr<float>(0, ch, row);
            uint8_t *ptrMaxCl = maxCl.ptr<uint8_t>(row);
            float *ptrMaxVal = maxVal.ptr<float>(row);
            for (int col = 0; col < cols; col++) {
                if (ptrScore[col] > ptrMaxVal[col]) {
                    ptrMaxVal[col] = ptrScore[col];
                    ptrMaxCl[col] = (uchar)ch;
                }
            }
        }
    }

    segm.create(rows, cols, CV_8UC3);
    for (int row = 0; row < rows; row++) {
        const uchar *ptrMaxCl = maxCl.ptr<uchar>(row);
        cv::Vec3b *ptrSegm = segm.ptr<cv::Vec3b>(row);
        for (int col = 0; col < cols; col++) {
            ptrSegm[col] = _seg_colors[ptrMaxCl[col]];
        }
    }
}

void cvedix_osd_node::show_seg_legend(cv::Mat &canvas) {
    if (_seg_classes.empty()) return;

    int kBlockHeight = 20;
    int legend_w = 120;
    int legend_h = kBlockHeight * (int)_seg_classes.size();
    int legend_x = 5;
    int legend_y = 5;

    if (legend_y + legend_h > canvas.rows) legend_h = canvas.rows - legend_y;

    // Semi-transparent background
    cv::Mat overlay = canvas.clone();
    cv::rectangle(overlay, cv::Rect(legend_x, legend_y, legend_w, legend_h),
                  cv::Scalar(0, 0, 0), cv::FILLED);
    cv::addWeighted(overlay, 0.5, canvas, 0.5, 0, canvas);

    int numClasses = std::min((int)_seg_classes.size(), (int)_seg_colors.size());
    for (int i = 0; i < numClasses && (legend_y + (i+1)*kBlockHeight) <= canvas.rows; i++) {
        cv::rectangle(canvas,
            cv::Rect(legend_x, legend_y + i * kBlockHeight, 15, kBlockHeight),
            cv::Scalar(_seg_colors[i][0], _seg_colors[i][1], _seg_colors[i][2]), cv::FILLED);
        cv::putText(canvas, _seg_classes[i],
            cv::Point(legend_x + 18, legend_y + (i + 1) * kBlockHeight - 4),
            cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(255, 255, 255), 1);
    }
}

void cvedix_osd_node::render_seg(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    cv::Mat segm;
    colorize_segmentation(meta->mask, segm);
    cv::resize(segm, segm, canvas.size(), 0, 0, cv::INTER_NEAREST);

    // Blend segmentation overlay on canvas (semi-transparent)
    cv::addWeighted(canvas, 0.5, segm, 0.5, 0.0, canvas);

    // Legend in top-left corner
    show_seg_legend(canvas);
}

// ═══════════════════════════════════════════
// MLLM Description Rendering (from mllm_osd — fixed canvas overlay)
// ═══════════════════════════════════════════

std::vector<std::string> cvedix_osd_node::utf8_split(const std::string &text) {
    std::vector<std::string> chars;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i];
        size_t len = 1;
        if ((c & 0x80) == 0x00) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        chars.push_back(text.substr(i, len));
        i += len;
    }
    return chars;
}

void cvedix_osd_node::draw_text_in_rect(cv::Mat &img, const std::string &text,
                                                 const cv::Rect &rect, int fontHeight,
                                                 cv::Scalar color) {
    if (ft2 == nullptr) return;

    std::vector<std::string> chars = utf8_split(text);
    std::string currentLine;
    int baseline = 0;
    int y = rect.y;

    for (size_t i = 0; i < chars.size(); i++) {
        std::string tempLine = currentLine + chars[i];
        cv::Size textSize = ft2->getTextSize(tempLine, fontHeight, -1, &baseline);

        if (textSize.width > rect.width && !currentLine.empty()) {
            int drawY = y + textSize.height;
            if (drawY > rect.y + rect.height) break;
            ft2->putText(img, currentLine, cv::Point(rect.x, drawY),
                         fontHeight, color, -1, 8, true);
            y += textSize.height + 5;
            currentLine.clear();
        }
        currentLine += chars[i];
    }

    if (!currentLine.empty()) {
        int drawY = y + ft2->getTextSize(currentLine, fontHeight, -1, &baseline).height;
        if (drawY <= rect.y + rect.height) {
            ft2->putText(img, currentLine, cv::Point(rect.x, drawY),
                         fontHeight, color, -1, 8, true);
        }
    }
}

void cvedix_osd_node::render_mllm(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    if (ft2 == nullptr) return;

    int gap_height = 100;
    int padding = 10;

    // Semi-transparent banner at bottom of frame
    int banner_y = canvas.rows - gap_height - padding;
    if (banner_y < 0) banner_y = 0;
    int banner_h = std::min(gap_height + padding, canvas.rows - banner_y);

    cv::Mat overlay = canvas.clone();
    cv::rectangle(overlay, cv::Rect(0, banner_y, canvas.cols, banner_h),
                  cv::Scalar(40, 40, 40), cv::FILLED);
    cv::addWeighted(overlay, 0.7, canvas, 0.3, 0, canvas);

    // Draw text in banner
    draw_text_in_rect(canvas, meta->description,
        cv::Rect(padding, banner_y + 5, canvas.cols - padding * 2, banner_h - 10),
        18, cv::Scalar(200, 200, 255));

    CVEDIX_INFO(cvedix_utils::string_format("[%s] [%s]", node_name.c_str(), meta->description.c_str()));
}

// ═══════════════════════════════════════════
// Sub-Target Thumbnails (from osd_v2 — fixed canvas overlay)
// ═══════════════════════════════════════════

void cvedix_osd_node::render_sub_thumbnails(
    cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {

    int gap_height = 80;
    int padding = 5;
    int base_left = padding;
    bool has_subs = false;

    for (auto &target : meta->targets) {
        if (!target->sub_targets.empty()) { has_subs = true; break; }
    }
    if (!has_subs) return;

    // Semi-transparent strip at bottom
    int strip_h = gap_height + padding * 2;
    int strip_y = canvas.rows - strip_h;
    if (strip_y < 0) return;

    cv::Mat overlay = canvas.clone();
    cv::rectangle(overlay, cv::Rect(0, strip_y, canvas.cols, strip_h),
                  cv::Scalar(128, 128, 128), cv::FILLED);
    cv::addWeighted(overlay, 0.6, canvas, 0.4, 0, canvas);

    for (auto &target : meta->targets) {
        for (auto &sub_target : target->sub_targets) {
            cv::rectangle(canvas, cv::Rect(sub_target->x, sub_target->y,
                sub_target->width, sub_target->height), cv::Scalar(255, 255, 255), 2);

            // Crop and resize sub target
            if (sub_target->x >= 0 && sub_target->y >= 0 &&
                sub_target->x + sub_target->width <= canvas.cols &&
                sub_target->y + sub_target->height <= canvas.rows &&
                sub_target->width > 0 && sub_target->height > 0) {

                auto ori = canvas(cv::Rect(sub_target->x, sub_target->y,
                    sub_target->width, sub_target->height));
                cv::Mat tmp = ori.clone();
                int offset_w = 0, offset_h = 0;
                if (tmp.rows > tmp.cols) {
                    cv::resize(tmp, tmp, cv::Size(int(float(gap_height) / tmp.rows * tmp.cols), gap_height));
                    offset_w = (gap_height - tmp.cols) / 2;
                } else {
                    cv::resize(tmp, tmp, cv::Size(gap_height, int(float(gap_height) / tmp.cols * tmp.rows)));
                    offset_h = (gap_height - tmp.rows) / 2;
                }

                int dest_x = base_left + offset_w;
                int dest_y = strip_y + padding + offset_h;
                if (dest_x + tmp.cols <= canvas.cols && dest_y + tmp.rows <= canvas.rows) {
                    auto roi = canvas(cv::Rect(dest_x, dest_y, tmp.cols, tmp.rows));
                    tmp.copyTo(roi);
                }

                // Line from target to thumbnail
                cv::line(canvas,
                    cv::Point(target->x + target->width / 2, target->y + target->height),
                    cv::Point(base_left + gap_height / 2, strip_y),
                    cv::Scalar(255, 0, 0), 2, cv::LINE_AA);

                // Label
                auto sub_label = cvedix_utils::string_split(sub_target->label, '_');
                if (sub_label.size() == 2 && ft2 != nullptr) {
                    ft2->putText(canvas, sub_label[1],
                        cv::Point(base_left + 5, canvas.rows - padding - 5),
                        20, cv::Scalar(0), cv::FILLED, cv::LINE_AA, true);
                }

                base_left += gap_height + padding;
            }
        }

        // Target bbox + label
        if (!target->sub_targets.empty()) {
            auto labels_to_display = target->primary_label;
            for (auto &label : target->secondary_labels) {
                labels_to_display += "/" + label;
            }
            cv::rectangle(canvas, cv::Rect(target->x, target->y, target->width, target->height),
                          cv::Scalar(255, 255, 0), 3);
            if (ft2 != nullptr) {
                ft2->putText(canvas, labels_to_display, cv::Point(target->x, target->y),
                             25, cv::Scalar(255, 0, 255), cv::FILLED, cv::LINE_AA, true);
            } else {
                cv::putText(canvas, labels_to_display, cv::Point(target->x, target->y),
                            1, 1, cv::Scalar(255, 0, 255));
            }
        }
    }
}

} // namespace cvedix_nodes
