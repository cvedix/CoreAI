#include "cvedix_ba_event_extraction_node.h"
#include <sstream>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>

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

// ─── Constructors ───────────────────────────────────────────────

cvedix_ba_event_extraction_node::cvedix_ba_event_extraction_node(
    std::string node_name,
    std::string instance_id,
    std::function<void(const std::string&)> event_publisher,
    bool include_crop_images)
    : cvedix_msg_broker_node(node_name, cvedix_broke_for::NORMAL, 50, 200),
      instance_id(instance_id),
      event_publisher(event_publisher),
      include_crop_images(include_crop_images) {

    this->initialized();

    if (event_publisher == nullptr) {
        CVEDIX_WARN(cvedix_utils::string_format(
            "[%s] Event publisher not set. Events will be logged only.",
            node_name.c_str()));
    }

    CVEDIX_INFO(cvedix_utils::string_format(
        "[%s] BA Event Extraction Node initialized (instance_id: %s, crop: %s)",
        node_name.c_str(), instance_id.c_str(),
        include_crop_images ? "true" : "false"));
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

// ─── Format: serialize ba_results to JSON ───────────────────────

void cvedix_ba_event_extraction_node::format_msg(
    const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta,
    std::string& msg) {

    // Skip frames with no BA results
    if (meta->ba_results.empty()) {
        msg = "";
        return;
    }

    try {
        // Build JSON array of events
        std::ostringstream oss;
        oss << "[";
        bool first_event = true;

        for (const auto& ba : meta->ba_results) {
            if (!first_event) oss << ",";
            first_event = false;

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
                // Use the first target that matches involved_target_ids
                const auto& t = ba->involve_target_details[0];
                oss << "\"location\":{";
                oss << "\"height\":" << t.location_h << ",";
                oss << "\"width\":" << t.location_w << ",";
                oss << "\"x\":" << t.location_x << ",";
                oss << "\"y\":" << t.location_y;
                oss << "},";
                oss << "\"object_class\":\"" << json_escape(t.object_class) << "\",";
                oss << "\"ref_tracking_id\":\"" << json_escape(t.ref_tracking_id) << "\",";
                if (include_crop_images && !t.crop.empty()) {
                    std::string crop_b64 = mat_to_base64_jpeg(t.crop);
                    if (!crop_b64.empty()) {
                        oss << "\"crop_image\":\"" << crop_b64 << "\",";
                    }
                }
            }

            // Timestamps
            oss << "\"system_datetime\":\"" << json_escape(ba->system_datetime) << "\",";
            oss << "\"system_timestamp\":" << std::fixed << std::setprecision(0)
                << ba->system_timestamp;

            oss << "}";
        }

        oss << "]";
        msg = oss.str();

    } catch (const std::exception& e) {
        CVEDIX_ERROR(cvedix_utils::string_format(
            "[%s] format_msg failed: %s", node_name.c_str(), e.what()));
        msg = "";
    }
}

// ─── Publish ────────────────────────────────────────────────────

void cvedix_ba_event_extraction_node::broke_msg(const std::string& msg) {
    if (msg.empty()) return;

    if (event_publisher != nullptr) {
        try {
            event_publisher(msg);
        } catch (const std::exception& e) {
            CVEDIX_ERROR(cvedix_utils::string_format(
                "[%s] Event publisher failed: %s", node_name.c_str(), e.what()));
        }
    } else {
        CVEDIX_DEBUG(cvedix_utils::string_format(
            "[%s] No publisher set, event: %s",
            node_name.c_str(), msg.substr(0, 200).c_str()));
    }
}

} // namespace cvedix_nodes
