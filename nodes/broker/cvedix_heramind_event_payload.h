#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "cvedix/nodes/ba/cvedix_ppe_association.h"
#include "cvedix/third_party/cpp_base64/base64.h"
#include "cvedix/third_party/nlohmann/json.hpp"

namespace cvedix_nodes {

/**
 * @brief HeraMind event payload builder for PPE violations.
 *
 * HeraMind ingests a JSON array of typed items, discriminated by `$id`
 * (see HeraMind `examples/heracam-rv1126b/sample-event.json`):
 *
 *   - `event-<name>`  the event itself, carrying event_id, instance_id,
 *                     ref_tracking_id, object_class and a normalized `location`
 *   - `attribute`     one `{name, value}` pair, correlated by ref_tracking_id
 *   - `crop`          the evidence image plus the location it was cut from
 *
 * Boxes crossing this boundary are normalized to [0, 1] against the source
 * frame, which is what HeraMind expects. Keeping this a pure function lets the
 * payload be unit-tested without a broker, an encoder or a pipeline.
 */

/// Frame-relative box in normalized [0, 1] coordinates.
struct ppe_normalized_box {
    double x = 0, y = 0, width = 0, height = 0;
};

/// Clamp a pixel box into the frame and normalize it to [0, 1].
/// Returns a zero box for a degenerate frame or an empty intersection, which
/// callers treat as "no location available".
inline ppe_normalized_box normalize_box(
    int x, int y, int width, int height, int frame_width, int frame_height) {
    if (frame_width <= 0 || frame_height <= 0) return {};
    const double left = std::clamp(static_cast<double>(x), 0.0, static_cast<double>(frame_width));
    const double top = std::clamp(static_cast<double>(y), 0.0, static_cast<double>(frame_height));
    const double right = std::clamp(
        static_cast<double>(x) + static_cast<double>(width), left, static_cast<double>(frame_width));
    const double bottom = std::clamp(
        static_cast<double>(y) + static_cast<double>(height), top, static_cast<double>(frame_height));
    return {left / frame_width, top / frame_height,
            (right - left) / frame_width, (bottom - top) / frame_height};
}

inline nlohmann::json to_json(const ppe_normalized_box& box) {
    return {{"x", box.x}, {"y", box.y}, {"width", box.width}, {"height", box.height}};
}

/// Milliseconds since the Unix epoch as an ISO 8601 UTC string
/// ("2026-07-27T14:10:53Z"), matching the `system_datetime` HeraMind stores.
inline std::string iso8601_utc(int64_t epoch_ms) {
    const std::time_t seconds = static_cast<std::time_t>(epoch_ms / 1000);
    std::tm utc{};
    if (gmtime_r(&seconds, &utc) == nullptr) return "";
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) return "";
    return std::string(buffer);
}

/**
 * @brief Deterministic UUID-shaped identifier for a (camera, track) pair.
 *
 * HeraMind correlates an event with its attributes and crop through
 * `ref_tracking_id`, so the value must stay identical for the same tracked
 * person across every event. A per-event random UUID would break that
 * correlation, and a bare integer would collide between cameras. Two FNV-1a
 * passes over "camera:track" fill the 128 bits; this is a stable identifier,
 * not a cryptographic UUID.
 */
inline std::string stable_tracking_id(const std::string& camera_id, int track_id) {
    const std::string key = camera_id + ':' + std::to_string(track_id);
    auto fnv1a = [](const std::string& data, uint64_t seed) {
        uint64_t hash = seed;
        for (const unsigned char byte : data) {
            hash ^= byte;
            hash *= 0x100000001b3ULL;
        }
        return hash;
    };
    const uint64_t high = fnv1a(key, 0xcbf29ce484222325ULL);
    const uint64_t low = fnv1a(key, 0x9e3779b97f4a7c15ULL);
    char buffer[37];
    std::snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx",
        static_cast<unsigned>(high >> 32),
        static_cast<unsigned>((high >> 16) & 0xffff),
        // Version 5 / variant 1 bits keep the shape valid for consumers that
        // parse it as a UUID.
        static_cast<unsigned>((high & 0x0fff) | 0x5000),
        static_cast<unsigned>(((low >> 48) & 0x3fff) | 0x8000),
        static_cast<unsigned long long>(low & 0xffffffffffffULL));
    return std::string(buffer);
}

/// Everything the builder needs; mirrors the fields the flat payload exposes.
struct ppe_event_payload_input {
    std::string camera_id;      ///< HeraMind `instance_id`
    std::string event_id;       ///< random UUID for this violation
    int track_id = -1;          ///< person track id, >= 0
    int mask = 0;               ///< 0 ok, 1 missing helmet, 2 missing vest, 3 both
    int channel_index = 0;
    int frame_index = 0;
    int frame_width = 0, frame_height = 0;
    int bbox_x = 0, bbox_y = 0, bbox_width = 0, bbox_height = 0;
    float confidence = 0.f;      ///< person detection score
    int64_t system_timestamp_ms = 0;
    int64_t event_timestamp_ms = 0;  ///< source time: frame_index / source_fps
    std::string object_class = "Person";
    std::vector<uchar> jpeg;     ///< person crop, JPEG encoded; may be empty
};

inline const char* ppe_event_status(int mask) { return ppe_status_label(mask); }

/**
 * @brief Build the HeraMind event array for one PPE violation.
 *
 * Emits the event item, one attribute per reported fact, and — when a JPEG
 * crop was supplied — the crop item carrying the base64 evidence image.
 * Boolean attributes use the "true"/"false" strings HeraMind's transform
 * examples normalise.
 */
inline std::string build_heramind_ppe_payload(const ppe_event_payload_input& in) {
    const std::string tracking_id = stable_tracking_id(in.camera_id, in.track_id);
    const std::string datetime = iso8601_utc(in.system_timestamp_ms);
    const auto location = normalize_box(in.bbox_x, in.bbox_y, in.bbox_width, in.bbox_height,
                                        in.frame_width, in.frame_height);
    const int missing_count = ((in.mask & 1) ? 1 : 0) + ((in.mask & 2) ? 1 : 0);

    // Correlation fields repeated on every item so each one stands alone.
    auto correlation = [&]() {
        return nlohmann::json{
            {"$version", 1},
            {"instance_id", in.camera_id},
            {"ref_tracking_id", tracking_id},
            {"event_timestamp_ms", in.event_timestamp_ms},
            {"system_timestamp", in.system_timestamp_ms},
            {"system_datetime", datetime}};
    };

    nlohmann::json missing = nlohmann::json::array();
    if (in.mask & 1) missing.push_back("helmet");
    if (in.mask & 2) missing.push_back("vest");

    nlohmann::json event = correlation();
    event["$id"] = "event-ppe-violation";
    event["event_id"] = in.event_id;
    event["object_class"] = in.object_class;
    event["location"] = to_json(location);
    event["location_space"] = "normalized";
    event["confidence"] = in.confidence;
    event["channel_index"] = in.channel_index;
    event["frame_index"] = in.frame_index;
    event["frame_width"] = in.frame_width;
    event["frame_height"] = in.frame_height;
    event["status"] = ppe_event_status(in.mask);
    event["has_helmet"] = (in.mask & 1) == 0;
    event["has_vest"] = (in.mask & 2) == 0;
    event["missing_ppe"] = missing;
    event["missing_ppe_count"] = missing_count;

    nlohmann::json payload = nlohmann::json::array();
    payload.push_back(std::move(event));

    auto attribute = [&](const std::string& name, const nlohmann::json& value) {
        nlohmann::json item = correlation();
        item["$id"] = "attribute";
        item["name"] = name;
        item["value"] = value;
        payload.push_back(std::move(item));
    };
    // Scalar booleans as strings: dashboard transforms read them directly.
    attribute("has_helmet", (in.mask & 1) == 0 ? "true" : "false");
    attribute("has_vest", (in.mask & 2) == 0 ? "true" : "false");
    attribute("missing_helmet", (in.mask & 1) != 0 ? "true" : "false");
    attribute("missing_vest", (in.mask & 2) != 0 ? "true" : "false");
    attribute("ppe_status", ppe_event_status(in.mask));
    attribute("person_confidence", in.confidence);
    attribute("track_id", in.track_id);
    attribute("frame_index", in.frame_index);

    if (!in.jpeg.empty()) {
        nlohmann::json crop = correlation();
        crop["$id"] = "crop";
        // Raw base64 (no data: URI prefix) — the form the HeraMind examples use.
        crop["image"] = base64_encode(in.jpeg.data(), in.jpeg.size());
        crop["image_encoding"] = "base64";
        crop["image_format"] = "jpeg";
        crop["confidence"] = in.confidence;
        crop["crop_timestamp_ms"] = in.event_timestamp_ms;
        crop["ref_event_id"] = in.event_id;
        crop["location"] = to_json(location);
        payload.push_back(std::move(crop));
    }

    return payload.dump();
}

} // namespace cvedix_nodes
