/**
 * @file cvedix_osd_node.h
 * @brief Unified On-Screen Display node
 *
 * Consolidates all OSD rendering into a single node with configurable layers.
 * Auto-detects which data fields are present in frame metadata and renders
 * accordingly. Each rendering layer can be toggled via unified_osd_config.
 *
 * Replaces: cvedix_osd_node, cvedix_ba_line_crossline_osd_node,
 *           cvedix_ba_area_crowding_osd_node, and future BA/domain OSD nodes.
 */

#pragma once

#include <map>
#include <set>
#include <vector>
#include <string>
#include <random>
#include <fstream>
#include <opencv2/freetype.hpp>
#include <opencv2/imgproc.hpp>
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/objects/shapes/cvedix_point.h"
#include "cvedix/objects/shapes/cvedix_line.h"
#include "cvedix/objects/ba/cvedix_ba_result.h"

namespace cvedix_nodes {

// ═══════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════

/**
 * @brief Configuration for the unified OSD node
 *
 * All rendering layers are toggleable. Defaults are optimized for
 * surveillance-style analytics (green dots, no bbox, ID-only labels).
 */
struct unified_osd_config {
    // ── Target rendering ──
    bool show_bbox          = false;    // bounding box rectangle
    bool show_label         = false;    // class label text (e.g. "person")
    bool show_track_id      = true;     // tracking ID number
    bool show_track_trail   = true;     // tracking path trail
    bool show_center_dot    = true;     // center point dot
    bool show_sub_targets   = false;    // sub-target bounding boxes

    // ── Style ──
    cv::Scalar dot_color       = {0, 255, 0};     // green
    cv::Scalar trail_color     = {0, 255, 0};      // green
    cv::Scalar bbox_color      = {255, 255, 0};    // cyan
    cv::Scalar label_color     = {179, 52, 255};   // purple
    cv::Scalar alert_color     = {0, 0, 255};      // red (crossed/alarm)
    int bbox_thickness         = 2;
    double label_font_scale    = 0.4;
    int dot_radius             = 5;

    // ── BA layers ──
    bool enable_ba_crossline   = true;
    bool enable_ba_crowding    = true;
    bool enable_ba_jam         = true;
    bool enable_ba_stop        = true;
    bool enable_ba_enter_exit  = true;

    // ── Domain layers (auto-detected from metadata) ──
    bool enable_face           = true;
    bool enable_pose           = true;
    bool enable_instance_mask  = true;   // osd_v3: per-target mask contour overlay
    bool enable_text_region    = true;   // text: polyline + text near detection
    bool enable_expr           = true;   // expr: ✓/× flags on text regions
    bool enable_lane           = true;   // lane: binary mask overlay
    bool enable_plate          = true;   // plate: colored plate bbox+label+history
    bool enable_seg            = true;   // seg: colorized segmentation overlay
    bool enable_mllm           = true;   // mllm: description text overlay
    bool enable_sub_thumbnails = true;   // osd_v2: sub-target thumbnail overlay
    float mask_threshold       = 0.5f;   // instance mask threshold (osd_v3)

    // ── Static geometry (always-on) ──
    bool show_static_lines     = true;
    bool show_static_zones     = true;
};

/**
 * @brief Line display config for crossline rendering
 */
struct unified_line_config {
    cvedix_objects::cvedix_line line;
    cv::Scalar color = cv::Scalar(0, 255, 0);
    std::string name;
    int crossing_count = 0;
    cvedix_objects::cvedix_ba_direct_type direction =
        cvedix_objects::cvedix_ba_direct_type::BOTH;

    unified_line_config() = default;
    unified_line_config(const cvedix_objects::cvedix_line &l,
                        const cv::Scalar &c = cv::Scalar(0, 255, 0),
                        const std::string &n = "",
                        cvedix_objects::cvedix_ba_direct_type dir =
                            cvedix_objects::cvedix_ba_direct_type::BOTH)
        : line(l), color(c), name(n), crossing_count(0), direction(dir) {}
};

/**
 * @brief Static line overlay config (for always-on geometry drawing)
 */
struct unified_static_line_config {
    cvedix_objects::cvedix_line line;
    cv::Scalar color = cv::Scalar(0, 255, 0);
    std::string name;

    unified_static_line_config() = default;
    unified_static_line_config(const cvedix_objects::cvedix_line &l,
                               const cv::Scalar &c = cv::Scalar(0, 255, 0),
                               const std::string &n = "")
        : line(l), color(c), name(n) {}
};

/**
 * @brief Static zone overlay config (for always-on polygon drawing)
 */
struct unified_static_zone_config {
    std::vector<cvedix_objects::cvedix_point> roi;
    cv::Scalar color = cv::Scalar(0, 200, 0);
    std::string name;

    unified_static_zone_config() = default;
    unified_static_zone_config(const std::vector<cvedix_objects::cvedix_point> &r,
                               const cv::Scalar &c = cv::Scalar(0, 200, 0),
                               const std::string &n = "")
        : roi(r), color(c), name(n) {}
};

// ═══════════════════════════════════════════
// Unified OSD Node
// ═══════════════════════════════════════════

/**
 * @brief Single OSD node that handles all rendering layers
 *
 * Usage:
 *   auto osd = std::make_shared<cvedix_osd_node>("osd_1");
 *   osd->update_config(cfg);                // optional: change defaults
 *   osd->set_static_lines(lines);           // optional: static geometry
 *   osd->set_static_zones(zones);           // optional: static zones
 *   osd->set_line_configs(0, crosslines);   // optional: crossline configs
 */
class cvedix_osd_node : public cvedix_node {

private:
    // FreeType font (single shared instance)
    cv::Ptr<cv::freetype::FreeType2> ft2;

    // Configuration
    unified_osd_config _config;

    // ── Static geometry ──
    std::vector<unified_static_line_config> _static_lines;
    std::vector<unified_static_zone_config> _static_zones;

    // ── BA Crossline state ──
    std::map<int, std::vector<unified_line_config>> _all_line_configs;
    std::map<int, int> _all_total_crossline;
    std::set<int> _all_crossed_track_ids;
    std::vector<cv::Scalar> _default_colors = {
        cv::Scalar(0, 255, 0),   cv::Scalar(0, 0, 255),
        cv::Scalar(255, 0, 0),   cv::Scalar(0, 255, 255),
        cv::Scalar(255, 0, 255), cv::Scalar(255, 255, 0),
        cv::Scalar(128, 0, 255), cv::Scalar(0, 128, 255)
    };

    // ── BA Jam state (per channel) ──
    std::map<int, std::vector<cvedix_objects::cvedix_point>> _jam_regions;
    std::map<int, bool> _jam_active;
    std::map<int, std::vector<int>> _jam_involve_ids;

    // ── BA Stop state (per channel) ──
    std::map<int, std::vector<cvedix_objects::cvedix_point>> _stop_regions;
    std::map<int, std::vector<int>> _stop_active_ids;

    // ── BA Enter/Exit state (per channel) ──
    struct _active_poly { std::vector<cvedix_objects::cvedix_point> poly; cv::Scalar color; int ttl; };
    struct _active_alert { std::string text; cv::Scalar color; int ttl; };
    std::map<int, std::vector<_active_poly>> _ee_polys;
    std::map<int, std::vector<_active_alert>> _ee_alerts;
    int _ee_ttl_frames = 50;

    // ── Pose skeleton pairs ──
    const std::map<cvedix_objects::cvedix_pose_type, std::vector<std::pair<int,int>>> _pose_pairs = {
        {cvedix_objects::cvedix_pose_type::body_25, {{1,8},{1,2},{1,5},{2,3},{3,4},{5,6},{6,7},{8,9},
            {9,10},{10,11},{8,12},{12,13},{13,14},{1,0},{0,15},{15,17},{0,16},{16,18},
            {14,19},{19,20},{14,21},{11,22},{22,23},{11,24}}},
        {cvedix_objects::cvedix_pose_type::coco, {{1,2},{1,5},{2,3},{3,4},{5,6},{6,7},
            {1,8},{8,9},{9,10},{1,11},{11,12},{12,13},{1,0},{0,14},{14,16},{0,15},{15,17},{2,16},{5,17}}},
        {cvedix_objects::cvedix_pose_type::mpi_15, {{0,1},{1,2},{2,3},{3,4},{1,5},{5,6},{6,7},{1,14},
            {14,8},{8,9},{9,10},{14,11},{11,12},{12,13},{0,2},{0,5}}},
        {cvedix_objects::cvedix_pose_type::yolov8_pose_17, {{0,1},{0,2},{0,5},{0,6},{1,2},{1,3},{2,4},
            {5,6},{5,7},{5,11},{6,8},{6,12},{7,9},{8,10},{11,12},{11,13},{12,14},{13,15},{14,16}}}
    };
    std::vector<cv::Scalar> _pose_colors;

    // ── Plate color maps ──
    const std::map<std::string, std::string> _plate_text_colors = {
        {"blue", "蓝"}, {"green", "绿"}, {"yellow", "黄"},
        {"white", "白"}, {"black", "黑"}, {"purplish", "紫"},
        {"pink", "粉"}, {"cyan", "青"}
    };
    const std::map<std::string, cv::Scalar> _plate_draw_colors = {
        {"blue", cv::Scalar(255, 0, 0)}, {"green", cv::Scalar(0, 255, 0)},
        {"yellow", cv::Scalar(0, 255, 255)}, {"white", cv::Scalar(255, 255, 255)},
        {"black", cv::Scalar(0, 0, 0)}, {"purplish", cv::Scalar(128, 0, 128)},
        {"pink", cv::Scalar(203, 192, 255)}, {"cyan", cv::Scalar(255, 255, 0)}
    };
    std::vector<cv::Mat> _plate_history;
    static constexpr int _plate_his_height = 80;
    static constexpr int _plate_his_gap = 5;

    // ── Segmentation state ──
    std::vector<cv::Vec3b> _seg_colors;
    std::vector<std::string> _seg_classes;

    // ── Rendering methods ──
    void render_targets(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_static_lines(cv::Mat &canvas);
    void render_static_zones(cv::Mat &canvas);
    void render_ba_crossline(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_ba_crowding(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_ba_jam(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_ba_stop(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_ba_enter_exit(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_face(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_pose(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_instance_mask(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_text_region(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_expr(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_lane(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_plate(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_seg(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_mllm(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);
    void render_sub_thumbnails(cv::Mat &canvas, std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta);

    // Helpers
    static void draw_direction_arrow(cv::Mat &canvas,
                                      const cvedix_objects::cvedix_line &line,
                                      const cv::Scalar &color,
                                      cvedix_objects::cvedix_ba_direct_type direction);
    void colorize_segmentation(const cv::Mat &score, cv::Mat &segm);
    void show_seg_legend(cv::Mat &canvas);
    static std::vector<std::string> utf8_split(const std::string &text);
    void draw_text_in_rect(cv::Mat &img, const std::string &text,
                           const cv::Rect &rect, int fontHeight, cv::Scalar color);

protected:
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
    virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(
        std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;

public:
    cvedix_osd_node(std::string node_name, std::string font = "");
    ~cvedix_osd_node();

    /** @brief Update rendering config at runtime */
    void update_config(const unified_osd_config &config);

    /** @brief Get current config */
    const unified_osd_config& get_config() const { return _config; }

    /** @brief Set static crosslines to draw every frame */
    void set_static_lines(const std::vector<unified_static_line_config> &lines);

    /** @brief Set static zones to draw every frame */
    void set_static_zones(const std::vector<unified_static_zone_config> &zones);

    /** @brief Set crossline display configs for a channel */
    void set_line_configs(int channel_id, const std::vector<unified_line_config> &configs);

    /** @brief Set color for a specific crossline */
    void set_line_color(int channel_id, int line_index, const cv::Scalar &color);

    /** @brief Set segmentation classes and colors for seg rendering */
    void set_seg_config(const std::vector<std::string> &classes,
                        const std::vector<cv::Vec3b> &colors);
};

} // namespace cvedix_nodes
