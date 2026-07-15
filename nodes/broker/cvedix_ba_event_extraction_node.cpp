#include "cvedix_ba_event_extraction_node.h"
#include <sstream>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace cvedix_nodes {

// ─── Schema ID mapping ──────────────────────────────────────────

std::string cvedix_ba_event_extraction_node::ba_type_to_schema_id(
    cvedix_objects::cvedix_ba_type type) {
    switch (type) {
        case cvedix_objects::cvedix_ba_type::CROWDING:         return "event-crowd-detection";
        case cvedix_objects::cvedix_ba_type::AREA_ENTER:       return "event-area-enter";
        case cvedix_objects::cvedix_ba_type::AREA_EXIT:        return "event-area-exit";
        case cvedix_objects::cvedix_ba_type::INTRUSION_START:  return "event-intrusion";
        case cvedix_objects::cvedix_ba_type::INTRUSION_END:    return "event-intrusion-end";
        case cvedix_objects::cvedix_ba_type::CROSSLINE:        return "event-crossline";
        case cvedix_objects::cvedix_ba_type::STOP:             return "event-stop";
        case cvedix_objects::cvedix_ba_type::UNSTOP:           return "event-stop-end";
        case cvedix_objects::cvedix_ba_type::LOITERING:        return "event-loitering";
        case cvedix_objects::cvedix_ba_type::LOITERING_END:    return "event-loitering-end";
        case cvedix_objects::cvedix_ba_type::DWELL:            return "event-dwelling";
        default: return "event-unknown";
    }
}

// ─── JSON helper (escape strings) ───────────────────────────────

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

// ─── Base64 encode helper ───────────────────────────────────────

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::vector<uchar>& data) {
    std::string result;
    int i = 0;
    int len = data.size();
    result.reserve(((len + 2) / 3) * 4);
    while (i < len) {
        uint32_t octet_a = i < len ? data[i++] : 0;
        uint32_t octet_b = i < len ? data[i++] : 0;
        uint32_t octet_c = i < len ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        result += b64_table[(triple >> 18) & 0x3F];
        result += b64_table[(triple >> 12) & 0x3F];
        result += (i > len + 1) ? '=' : b64_table[(triple >> 6) & 0x3F];
        result += (i > len) ? '=' : b64_table[triple & 0x3F];
    }
    return result;
}

static std::string mat_to_base64_jpeg(const cv::Mat& img) {
    if (img.empty()) return "";
    std::vector<uchar> buf;
    cv::imencode(".jpg", img, buf, {cv::IMWRITE_JPEG_QUALITY, 85});
    return base64_encode(buf);
}

static cv::Rect to_frame_rect(const cvedix_objects::involved_target_info& t,
                              int frame_w,
                              int frame_h) {
    double x = t.location_x;
    double y = t.location_y;
    double w = t.location_w;
    double h = t.location_h;

    // Some pipelines store BA coordinates in 0..10000 normalized space.
    if (frame_w > 0 && frame_h > 0 && x >= 0 && y >= 0 && w > 0 && h > 0 &&
        (x > frame_w || y > frame_h || w > frame_w || h > frame_h)) {
        const double scale_x = static_cast<double>(frame_w) / 10000.0;
        const double scale_y = static_cast<double>(frame_h) / 10000.0;
        x *= scale_x;
        y *= scale_y;
        w *= scale_x;
        h *= scale_y;
    }

    int ix = std::max(0, static_cast<int>(std::round(x)));
    int iy = std::max(0, static_cast<int>(std::round(y)));
    int iw = std::max(1, static_cast<int>(std::round(w)));
    int ih = std::max(1, static_cast<int>(std::round(h)));

    if (ix >= frame_w || iy >= frame_h) {
        return cv::Rect();
    }

    if (ix + iw > frame_w) {
        iw = frame_w - ix;
    }
    if (iy + ih > frame_h) {
        ih = frame_h - iy;
    }

    if (iw <= 0 || ih <= 0) {
        return cv::Rect();
    }

    return cv::Rect(ix, iy, iw, ih);
}

static std::string build_event_bbox_overlay_base64(
    const cv::Mat& frame,
    const std::vector<cvedix_objects::involved_target_info>& targets) {
    if (frame.empty()) {
        return "";
    }

    cv::Mat overlay = frame.clone();
    const cv::Scalar color(0, 0, 255);

    for (const auto& t : targets) {
        const cv::Rect rect = to_frame_rect(t, overlay.cols, overlay.rows);
        if (rect.empty()) {
            continue;
        }
        cv::rectangle(overlay, rect, color, 2);
    }

    return mat_to_base64_jpeg(overlay);
}

static std::vector<cvedix_objects::involved_target_info> select_overlay_targets(
    const std::shared_ptr<cvedix_objects::cvedix_ba_result>& ba) {
    std::vector<cvedix_objects::involved_target_info> selected;
    if (!ba) {
        return selected;
    }

    if (ba->involve_target_details.empty()) {
        return selected;
    }

    // Group events keep all involved targets.
    if (ba->type == cvedix_objects::cvedix_ba_type::CROWDING) {
        return ba->involve_target_details;
    }

    // Non-group events should highlight only the primary trigger target.
    if (!ba->involve_target_ids_in_frame.empty()) {
        const int primary_track_id = ba->involve_target_ids_in_frame.front();
        for (const auto& detail : ba->involve_target_details) {
            if (detail.track_id == primary_track_id) {
                selected.push_back(detail);
                return selected;
            }
        }
    }

    selected.push_back(ba->involve_target_details.front());
    return selected;
}

// ─── Serialize a single BA result to JSON ──────────────────────

std::string cvedix_ba_event_extraction_node::serialize_event(
    const std::shared_ptr<cvedix_objects::cvedix_ba_result>& ba,
    const cv::Mat& frame) const {

    std::ostringstream oss;
    std::string schema_id = ba_type_to_schema_id(ba->type);

    oss << "{";
    oss << "\"$id\":\"" << json_escape(schema_id) << "\",";
    oss << "\"$version\":1,";

    // Area / region info
    oss << "\"area_id\":\"" << json_escape(ba->region_id) << "\",";
    oss << "\"area_name\":\"" << json_escape(ba->region_name) << "\",";

    // Event identifiers
    oss << "\"event_id\":\"" << json_escape(ba->event_id) << "\",";
    oss << "\"event_timestamp_ms\":" << ba->event_timestamp_ms << ",";

    // Duration (only for end events)
    if (ba->event_duration_ms > 0) {
        oss << "\"event_duration_ms\":" << std::fixed << std::setprecision(0)
            << ba->event_duration_ms << ",";
    }

    // Instance ID
    if (!instance_id.empty()) {
        oss << "\"instance_id\":\"" << json_escape(instance_id) << "\",";
    }

    // Check if this is a group event (crowding) or single-target event
    bool is_group_event = (ba->type == cvedix_objects::cvedix_ba_type::CROWDING);

    if (is_group_event && !ba->involve_target_details.empty()) {
        // Group event: targets array
        oss << "\"targets\":[";
        bool first_target = true;
        for (const auto& t : ba->involve_target_details) {
            if (!first_target) oss << ",";
            first_target = false;
            oss << "{";
            oss << "\"location\":{";
            oss << "\"height\":" << t.location_h << ",";
            oss << "\"width\":" << t.location_w << ",";
            oss << "\"x\":" << t.location_x << ",";
            oss << "\"y\":" << t.location_y;
            oss << "},";
            oss << "\"object_class\":\"" << json_escape(t.object_class) << "\",";
            oss << "\"ref_tracking_id\":\"" << json_escape(t.ref_tracking_id) << "\"";
            if (include_crop_images && !t.crop.empty()) {
                std::string crop_b64 = mat_to_base64_jpeg(t.crop);
                if (!crop_b64.empty()) {
                    oss << ",\"crop_image\":\"" << crop_b64 << "\"";
                }
            }
            oss << "}";
        }
        oss << "],";
    } else if (!ba->involve_target_details.empty()) {
        // Single-target event: flat location at top level
        const auto& t = ba->involve_target_details[0];
        oss << "\"location\":{";
        oss << "\"height\":" << t.location_h << ",";
        oss << "\"width\":" << t.location_w << ",";
        oss << "\"x\":" << t.location_x << ",";
        oss << "\"y\":" << t.location_y;
        oss << "},";
        oss << "\"object_class\":\"" << json_escape(t.object_class) << "\",";
        oss << "\"ref_tracking_id\":\"" << json_escape(t.ref_tracking_id) << "\"";
        if (include_crop_images && !t.crop.empty()) {
            std::string crop_b64 = mat_to_base64_jpeg(t.crop);
            if (!crop_b64.empty()) {
                oss << ",\"crop_image\":\"" << crop_b64 << "\"";
            }
        }
        oss << ",";
    }

    // Timestamps
    if (include_full_frame_images && !frame.empty()) {
        const std::string full_frame_b64 = mat_to_base64_jpeg(frame);
        if (!full_frame_b64.empty()) {
            oss << "\"full_frame_image\":\"" << full_frame_b64 << "\",";
            const std::string bbox_frame_b64 =
                build_event_bbox_overlay_base64(frame, select_overlay_targets(ba));
            if (!bbox_frame_b64.empty()) {
                oss << "\"full_frame_bbox_image\":\"" << bbox_frame_b64 << "\",";
            }
        }
    }

    oss << "\"system_datetime\":\"" << json_escape(ba->system_datetime) << "\",";
    oss << "\"system_timestamp\":" << std::fixed << std::setprecision(0)
        << ba->system_timestamp;

    oss << "}";
    return oss.str();
}

// ─── Constructors ───────────────────────────────────────────────

cvedix_ba_event_extraction_node::cvedix_ba_event_extraction_node(
    std::string node_name,
    std::string instance_id,
    std::function<void(const std::string&)> event_publisher,
        bool include_crop_images,
        bool include_full_frame_images)
    : cvedix_msg_broker_node(node_name, cvedix_broke_for::NORMAL, 50, 200),
      instance_id(instance_id),
      event_publisher(event_publisher),
            include_crop_images(include_crop_images),
            include_full_frame_images(include_full_frame_images) {

    this->initialized();

    if (event_publisher == nullptr) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Event publisher not set. Events will be logged only.",
            node_name.c_str()));
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] BA Event Extraction Node initialized (instance_id: %s, crop: %s, full_frame: %s)",
        node_name.c_str(), instance_id.c_str(),
        include_crop_images ? "true" : "false",
        include_full_frame_images ? "true" : "false"));
}

cvedix_ba_event_extraction_node::~cvedix_ba_event_extraction_node() {
    deinitialized();
    stop_broking();
}

void cvedix_ba_event_extraction_node::set_event_publisher(
    std::function<void(const std::string&)> publisher) {
    event_publisher = publisher;
}

void cvedix_ba_event_extraction_node::set_instance_id(const std::string& id) {
    instance_id = id;
}

// ─── Push event directly (bypass queue) ─────────────────────────
// Suitable for SSE/webhook where immediate sending is preferred.

void cvedix_ba_event_extraction_node::push_event(const std::string& event_json) {
    if (event_publisher != nullptr) {
        try {
            event_publisher(event_json);
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Event publisher failed: %s", node_name.c_str(), e.what()));
        }
    } else {
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] No publisher set, event: %s",
            node_name.c_str(), event_json.substr(0, 200).c_str()));
    }
}

// ─── Format: serialize ba_results to JSON array ───────────────
// Implements broker pattern: builds JSON, sets msg, returns.
// The broker thread will call broke_msg(msg) to publish.

void cvedix_ba_event_extraction_node::format_msg(
    const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta,
    std::string& msg) {

    // Skip frames with no BA results
    if (meta->ba_results.empty()) {
        msg = "";
        return;
    }

    try {
        std::ostringstream oss;
        oss << "[";

        bool first = true;
        for (const auto& ba : meta->ba_results) {
            if (!first) oss << ",";
            first = false;
            oss << serialize_event(ba, meta->frame);
        }

        oss << "]";
        msg = oss.str();

    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] format_msg failed: %s", node_name.c_str(), e.what()));
        msg = "";
    }
}

// ─── Publish via broker thread ─────────────────────────────────
// Called by broker thread (broking_run) after format_msg.
// Triggers event_publisher callback with the JSON array.

void cvedix_ba_event_extraction_node::broke_msg(const std::string& msg) {
    if (msg.empty()) return;

    if (event_publisher != nullptr) {
        try {
            event_publisher(msg);
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] MQTT publisher function failed: %s",
                node_name.c_str(), e.what()));
        } catch (...) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] MQTT publisher function failed with unknown error",
                node_name.c_str()));
        }
    } else {
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] MQTT publisher not set, message: %s",
            node_name.c_str(), msg.substr(0, 200).c_str()));
    }
}

} // namespace cvedix_nodes
