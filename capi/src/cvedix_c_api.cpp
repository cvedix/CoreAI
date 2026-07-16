/**
 * @file cvedix_c_api.cpp
 * @brief Implementation of the CVEDIX C API (see cvedix_c_api.h).
 */
#include "cvedix_c_api.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/src/cvedix_rtsp_src_node.h"
#include "cvedix/nodes/infers/cvedix_yolo_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/track/cvedix_bytetrack_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/des/cvedix_rtsp_des_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/nodes/des/cvedix_app_des_node.h"
#include "cvedix/objects/cvedix_frame_meta.h"
#include "cvedix/utils/logger/cvedix_logger.h"
#include "cvedix_version.h"

/* ── Internal wrapper ──────────────────────────────────────────────────── */

struct cvedix_node {
    std::shared_ptr<cvedix_nodes::cvedix_node> node;

    // Only used for app destination nodes:
    cvedix_result_callback_t callback = nullptr;
    void* user_data = nullptr;
};

namespace {

thread_local std::string g_last_error;

void set_error(const std::string& msg) { g_last_error = msg; }

std::string safe_str(const char* s, const char* fallback = "") {
    return s ? std::string(s) : std::string(fallback);
}

template <typename Fn>
cvedix_node_t* create_wrapped(Fn&& make) {
    try {
        auto wrapper = new cvedix_node();
        wrapper->node = make();
        return wrapper;
    } catch (const std::exception& e) {
        set_error(e.what());
        return nullptr;
    } catch (...) {
        set_error("unknown error while creating node");
        return nullptr;
    }
}

} // namespace

/* ── Runtime ───────────────────────────────────────────────────────────── */

extern "C" {

int cvedix_init(cvedix_log_level_t log_level) {
    try {
        CVEDIX_SET_LOG_LEVEL(static_cast<cvedix_utils::cvedix_log_level>(log_level));
        CVEDIX_LOGGER_INIT();
        return CVEDIX_OK;
    } catch (const std::exception& e) {
        set_error(e.what());
        return CVEDIX_ERR_EXCEPTION;
    }
}

const char* cvedix_version(void) { return CVEDIX_VERSION; }

const char* cvedix_last_error(void) { return g_last_error.c_str(); }

/* ── Sources ───────────────────────────────────────────────────────────── */

cvedix_node_t* cvedix_file_src_create(
    const char* name, int channel, const char* file_path,
    float resize_ratio, int loop, const char* gst_decoder) {
    if (!name || !file_path) { set_error("name/file_path must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            name, channel, file_path, resize_ratio, loop != 0,
            safe_str(gst_decoder, "avdec_h264"));
    });
}

cvedix_node_t* cvedix_rtsp_src_create(
    const char* name, int channel, const char* rtsp_url,
    float resize_ratio, const char* gst_decoder) {
    if (!name || !rtsp_url) { set_error("name/rtsp_url must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
            name, channel, rtsp_url, resize_ratio,
            safe_str(gst_decoder, "avdec_h264"));
    });
}

/* ── Inference ─────────────────────────────────────────────────────────── */

cvedix_node_t* cvedix_yolo_detector_create(
    const char* name, const char* model_path, cvedix_yolo_version_t version,
    const char* labels_path, float conf_threshold, float nms_threshold) {
    if (!name || !model_path) { set_error("name/model_path must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
            name, model_path,
            static_cast<cvedix_nodes::YoloVersion>(version),
            safe_str(labels_path), conf_threshold, nms_threshold);
    });
}

/* ── Trackers ──────────────────────────────────────────────────────────── */

cvedix_node_t* cvedix_sort_tracker_create(const char* name) {
    if (!name) { set_error("name must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_sort_track_node>(name);
    });
}

cvedix_node_t* cvedix_bytetrack_tracker_create(
    const char* name, float track_thresh, float high_thresh,
    float match_thresh, int track_buffer, int frame_rate) {
    if (!name) { set_error("name must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_bytetrack_node>(
            name, cvedix_nodes::cvedix_track_for::NORMAL,
            track_thresh, high_thresh, match_thresh, track_buffer, frame_rate);
    });
}

/* ── OSD ───────────────────────────────────────────────────────────────── */

cvedix_node_t* cvedix_osd_create(const char* name, const char* font) {
    if (!name) { set_error("name must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_osd_node>(name, safe_str(font));
    });
}

/* ── Destinations ──────────────────────────────────────────────────────── */

cvedix_node_t* cvedix_rtsp_des_create(
    const char* name, int channel, int port,
    const char* stream_name, int bitrate_kbps, int draw_osd) {
    if (!name) { set_error("name must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_rtsp_des_node>(
            name, channel, port, safe_str(stream_name),
            cvedix_objects::cvedix_size{}, bitrate_kbps, draw_osd != 0);
    });
}

cvedix_node_t* cvedix_file_des_create(
    const char* name, int channel, const char* save_dir,
    const char* name_prefix, int max_minutes_per_file,
    int bitrate_kbps, int draw_osd) {
    if (!name || !save_dir) { set_error("name/save_dir must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_file_des_node>(
            name, channel, save_dir, safe_str(name_prefix),
            max_minutes_per_file, cvedix_objects::cvedix_size{},
            bitrate_kbps, draw_osd != 0);
    });
}

cvedix_node_t* cvedix_web_des_create(
    const char* name, int channel, int port, int jpeg_quality) {
    if (!name) { set_error("name must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
            name, channel, port, nullptr, jpeg_quality);
    });
}

cvedix_node_t* cvedix_app_des_create(const char* name, int channel) {
    if (!name) { set_error("name must not be NULL"); return nullptr; }
    return create_wrapped([&] {
        return std::make_shared<cvedix_nodes::cvedix_app_des_node>(name, channel);
    });
}

/* ── Result callback ───────────────────────────────────────────────────── */

int cvedix_app_des_set_callback(
    cvedix_node_t* app_des, cvedix_result_callback_t callback, void* user_data) {
    if (!app_des || !callback) { set_error("app_des/callback must not be NULL"); return CVEDIX_ERR_INVALID; }
    auto typed = std::dynamic_pointer_cast<cvedix_nodes::cvedix_app_des_node>(app_des->node);
    if (!typed) { set_error("handle is not an app destination node"); return CVEDIX_ERR_NOT_APPDES; }

    app_des->callback = callback;
    app_des->user_data = user_data;

    try {
        typed->set_app_des_result_hooker(
            [app_des](std::string /*node_name*/,
                      std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
                if (!meta || meta->meta_type != cvedix_objects::cvedix_meta_type::FRAME) return;
                auto frame = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
                if (!frame) return;

                std::vector<cvedix_detection_t> dets;
                dets.reserve(frame->targets.size());
                for (const auto& t : frame->targets) {
                    if (!t) continue;
                    cvedix_detection_t d{};
                    d.x = t->x;
                    d.y = t->y;
                    d.width = t->width;
                    d.height = t->height;
                    d.class_id = t->primary_class_id;
                    d.score = t->primary_score;
                    d.track_id = t->track_id;
                    std::strncpy(d.label, t->primary_label.c_str(), sizeof(d.label) - 1);
                    dets.push_back(d);
                }
                app_des->callback(
                    frame->frame_index, frame->channel_index,
                    frame->original_width, frame->original_height,
                    dets.data(), static_cast<int32_t>(dets.size()),
                    app_des->user_data);
            });
        return CVEDIX_OK;
    } catch (const std::exception& e) {
        set_error(e.what());
        return CVEDIX_ERR_EXCEPTION;
    }
}

/* ── Pipeline wiring & lifecycle ───────────────────────────────────────── */

int cvedix_node_attach_to(cvedix_node_t* node, cvedix_node_t** upstreams, int count) {
    if (!node || !upstreams || count <= 0) { set_error("invalid attach arguments"); return CVEDIX_ERR_INVALID; }
    try {
        std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pre;
        pre.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            if (!upstreams[i]) { set_error("upstream handle is NULL"); return CVEDIX_ERR_INVALID; }
            pre.push_back(upstreams[i]->node);
        }
        node->node->attach_to(pre);
        return CVEDIX_OK;
    } catch (const std::exception& e) {
        set_error(e.what());
        return CVEDIX_ERR_EXCEPTION;
    }
}

int cvedix_node_detach(cvedix_node_t* node) {
    if (!node) { set_error("node is NULL"); return CVEDIX_ERR_INVALID; }
    try { node->node->detach(); return CVEDIX_OK; }
    catch (const std::exception& e) { set_error(e.what()); return CVEDIX_ERR_EXCEPTION; }
}

int cvedix_node_detach_recursively(cvedix_node_t* node) {
    if (!node) { set_error("node is NULL"); return CVEDIX_ERR_INVALID; }
    try { node->node->detach_recursively(); return CVEDIX_OK; }
    catch (const std::exception& e) { set_error(e.what()); return CVEDIX_ERR_EXCEPTION; }
}

int cvedix_src_start(cvedix_node_t* src) {
    if (!src) { set_error("src is NULL"); return CVEDIX_ERR_INVALID; }
    auto typed = std::dynamic_pointer_cast<cvedix_nodes::cvedix_src_node>(src->node);
    if (!typed) { set_error("handle is not a source node"); return CVEDIX_ERR_NOT_SOURCE; }
    try { typed->start(); return CVEDIX_OK; }
    catch (const std::exception& e) { set_error(e.what()); return CVEDIX_ERR_EXCEPTION; }
}

int cvedix_src_stop(cvedix_node_t* src) {
    if (!src) { set_error("src is NULL"); return CVEDIX_ERR_INVALID; }
    auto typed = std::dynamic_pointer_cast<cvedix_nodes::cvedix_src_node>(src->node);
    if (!typed) { set_error("handle is not a source node"); return CVEDIX_ERR_NOT_SOURCE; }
    try { typed->stop(); return CVEDIX_OK; }
    catch (const std::exception& e) { set_error(e.what()); return CVEDIX_ERR_EXCEPTION; }
}

int cvedix_node_destroy(cvedix_node_t* node) {
    if (!node) { set_error("node is NULL"); return CVEDIX_ERR_INVALID; }
    delete node;
    return CVEDIX_OK;
}

} /* extern "C" */
