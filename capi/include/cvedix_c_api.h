/**
 * @file cvedix_c_api.h
 * @brief CVEDIX C API — stable C ABI over the C++ Core Runtime.
 *
 * This is the foundation for all language bindings (C, Java/JNA, C#/P-Invoke).
 * Keep this header pure C (C99). Never expose C++ types here.
 *
 * Threading: callbacks are invoked from internal pipeline threads.
 * Ownership: every *_create() returns a handle that must be released with
 *            cvedix_node_destroy(). Destroy sinks/mids before sources, or call
 *            cvedix_src_stop() first.
 */
#ifndef CVEDIX_C_API_H
#define CVEDIX_C_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  define CVEDIX_API __declspec(dllexport)
#else
#  define CVEDIX_API __attribute__((visibility("default")))
#endif

#define CVEDIX_CAPI_VERSION_MAJOR 1
#define CVEDIX_CAPI_VERSION_MINOR 0

/* ── Status codes ──────────────────────────────────────────────────────── */
typedef enum cvedix_status {
    CVEDIX_OK             = 0,
    CVEDIX_ERR_INVALID    = -1, /* invalid argument / null handle          */
    CVEDIX_ERR_EXCEPTION  = -2, /* C++ exception, see cvedix_last_error()  */
    CVEDIX_ERR_NOT_SOURCE = -3, /* handle is not a source node             */
    CVEDIX_ERR_NOT_APPDES = -4  /* handle is not an app destination node   */
} cvedix_status_t;

/* ── Log level (mirrors cvedix_utils::cvedix_log_level) ───────────────── */
typedef enum cvedix_log_level {
    CVEDIX_LOG_ERROR = 1,
    CVEDIX_LOG_WARN  = 2,
    CVEDIX_LOG_INFO  = 3,
    CVEDIX_LOG_DEBUG = 4
} cvedix_log_level_t;

/* ── YOLO version (mirrors cvedix_nodes::YoloVersion) ──────────────────── */
typedef enum cvedix_yolo_version {
    CVEDIX_YOLO11  = 0,
    CVEDIX_YOLO26  = 1,
    CVEDIX_YOLO12  = 2,
    CVEDIX_RF_DETR = 3
} cvedix_yolo_version_t;

/* ── Opaque node handle ────────────────────────────────────────────────── */
typedef struct cvedix_node cvedix_node_t;

/* ── Runtime ───────────────────────────────────────────────────────────── */

/** Initialise logger. Call once before creating nodes. */
CVEDIX_API int cvedix_init(cvedix_log_level_t log_level);

/** Core runtime version string (static storage, do not free). */
CVEDIX_API const char* cvedix_version(void);

/** Last error message for the calling thread (static storage, do not free). */
CVEDIX_API const char* cvedix_last_error(void);

/* ── Source nodes ──────────────────────────────────────────────────────── */

/**
 * Video-file source.
 * @param gst_decoder  GStreamer decoder, e.g. "avdec_h264". NULL = default.
 */
CVEDIX_API cvedix_node_t* cvedix_file_src_create(
    const char* name, int channel, const char* file_path,
    float resize_ratio, int loop, const char* gst_decoder);

/** RTSP source. @param gst_decoder NULL = default "avdec_h264". */
CVEDIX_API cvedix_node_t* cvedix_rtsp_src_create(
    const char* name, int channel, const char* rtsp_url,
    float resize_ratio, const char* gst_decoder);

/* ── Inference nodes ───────────────────────────────────────────────────── */

/**
 * YOLO detector (backend auto-detected from model extension:
 * .onnx → ONNX Runtime, .engine → TensorRT, .rknn → RKNN).
 * @param labels_path  NULL = no label file (numeric class ids only).
 */
CVEDIX_API cvedix_node_t* cvedix_yolo_detector_create(
    const char* name, const char* model_path, cvedix_yolo_version_t version,
    const char* labels_path, float conf_threshold, float nms_threshold);

/* ── Tracker nodes ─────────────────────────────────────────────────────── */

/** SORT tracker (lightweight, no model needed). */
CVEDIX_API cvedix_node_t* cvedix_sort_tracker_create(const char* name);

/** ByteTrack tracker. Typical values: 0.5, 0.6, 0.8, 30, 30. */
CVEDIX_API cvedix_node_t* cvedix_bytetrack_tracker_create(
    const char* name, float track_thresh, float high_thresh,
    float match_thresh, int track_buffer, int frame_rate);

/* ── OSD node ──────────────────────────────────────────────────────────── */

/** On-screen-display (draw boxes/labels). @param font NULL = default font. */
CVEDIX_API cvedix_node_t* cvedix_osd_create(const char* name, const char* font);

/* ── Destination nodes ─────────────────────────────────────────────────── */

/** RTSP restream output. @param stream_name NULL = node name. */
CVEDIX_API cvedix_node_t* cvedix_rtsp_des_create(
    const char* name, int channel, int port,
    const char* stream_name, int bitrate_kbps, int draw_osd);

/** Segmented video-file output. @param name_prefix NULL = "". */
CVEDIX_API cvedix_node_t* cvedix_file_des_create(
    const char* name, int channel, const char* save_dir,
    const char* name_prefix, int max_minutes_per_file,
    int bitrate_kbps, int draw_osd);

/** Browser debug output (MJPEG + stats) at http://localhost:{port}. */
CVEDIX_API cvedix_node_t* cvedix_web_des_create(
    const char* name, int channel, int port, int jpeg_quality);

/** Application destination — delivers results via callback. */
CVEDIX_API cvedix_node_t* cvedix_app_des_create(const char* name, int channel);

/* ── Result callback (app destination) ─────────────────────────────────── */

/** One detected/tracked object. */
typedef struct cvedix_detection {
    int32_t x;          /* bounding box, pixels in original frame */
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t class_id;
    float   score;      /* confidence [0,1] */
    int32_t track_id;   /* -1 when no tracker in pipeline */
    char    label[64];  /* NUL-terminated class label */
} cvedix_detection_t;

/**
 * Invoked once per frame arriving at an app destination.
 * @param detections  array valid ONLY during the call — copy if needed.
 */
typedef void (*cvedix_result_callback_t)(
    int32_t frame_index, int32_t channel,
    int32_t frame_width, int32_t frame_height,
    const cvedix_detection_t* detections, int32_t count,
    void* user_data);

/** Register callback on an app destination node. */
CVEDIX_API int cvedix_app_des_set_callback(
    cvedix_node_t* app_des, cvedix_result_callback_t callback, void* user_data);

/* ── Pipeline wiring & lifecycle ───────────────────────────────────────── */

/** Attach node downstream of one or more upstream nodes. */
CVEDIX_API int cvedix_node_attach_to(
    cvedix_node_t* node, cvedix_node_t** upstreams, int count);

/** Detach node from its upstream(s). */
CVEDIX_API int cvedix_node_detach(cvedix_node_t* node);

/** Detach this node and everything downstream (call on the source). */
CVEDIX_API int cvedix_node_detach_recursively(cvedix_node_t* node);

/** Start frame generation (source nodes only). */
CVEDIX_API int cvedix_src_start(cvedix_node_t* src);

/** Stop frame generation (source nodes only). */
CVEDIX_API int cvedix_src_stop(cvedix_node_t* src);

/** Release a node handle. The node keeps living while attached elsewhere. */
CVEDIX_API int cvedix_node_destroy(cvedix_node_t* node);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CVEDIX_C_API_H */
