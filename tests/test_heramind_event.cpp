#include "test_assert.h"
#include "cvedix/nodes/broker/cvedix_heramind_event_payload.h"
#include "cvedix/third_party/cpp_base64/base64.h"
#include "cvedix/third_party/nlohmann/json.hpp"
#include <string>
#include <vector>

using namespace cvedix_nodes;

namespace {

/// A representative violation: a person in the left half of a 1280x720 frame.
ppe_event_payload_input sample_input(int mask = 1) {
    ppe_event_payload_input in;
    in.camera_id = "camera-01";
    in.event_id = "11111111-2222-3333-4444-555555555555";
    in.track_id = 7;
    in.mask = mask;
    in.channel_index = 0;
    in.frame_index = 300;
    in.frame_width = 1280;
    in.frame_height = 720;
    in.bbox_x = 100;
    in.bbox_y = 90;
    in.bbox_width = 320;
    in.bbox_height = 480;
    in.confidence = 0.87f;
    in.system_timestamp_ms = 1700000000000;   // 2023-11-14T22:13:20Z
    in.event_timestamp_ms = 20000;            // frame 300 at 15 fps
    in.jpeg = {'j', 'p', 'e', 'g', '-', 'b', 'y', 't', 'e', 's'};
    return in;
}

/// Find the single item carrying @p id, or nullptr when absent.
const nlohmann::json* find_item(const nlohmann::json& payload, const std::string& id) {
    for (const auto& item : payload) {
        if (item.value("$id", "") == id) return &item;
    }
    return nullptr;
}

/// All `attribute` items, keyed by name.
std::vector<std::pair<std::string, nlohmann::json>> attributes(const nlohmann::json& payload) {
    std::vector<std::pair<std::string, nlohmann::json>> found;
    for (const auto& item : payload) {
        if (item.value("$id", "") != "attribute") continue;
        found.emplace_back(item["name"].get<std::string>(), item["value"]);
    }
    return found;
}

bool has_attribute(const nlohmann::json& payload, const std::string& name,
                   const nlohmann::json& value) {
    for (const auto& [key, val] : attributes(payload)) {
        if (key == name) return val == value;
    }
    return false;
}

} // namespace

CVEDIX_TEST_CASE(box_is_clamped_to_the_frame_then_normalized) {
    const auto box = normalize_box(100, 90, 320, 480, 1280, 720);
    CVEDIX_ASSERT_TRUE(std::abs(box.x - 100.0 / 1280) < 1e-9);
    CVEDIX_ASSERT_TRUE(std::abs(box.y - 90.0 / 720) < 1e-9);
    CVEDIX_ASSERT_TRUE(std::abs(box.width - 320.0 / 1280) < 1e-9);
    CVEDIX_ASSERT_TRUE(std::abs(box.height - 480.0 / 720) < 1e-9);

    // A box hanging off the frame is trimmed, never reported outside [0, 1].
    const auto clipped = normalize_box(1200, 600, 400, 400, 1280, 720);
    CVEDIX_ASSERT_TRUE(clipped.x + clipped.width <= 1.0 + 1e-9);
    CVEDIX_ASSERT_TRUE(clipped.y + clipped.height <= 1.0 + 1e-9);

    // A negative origin is trimmed too.
    const auto negative = normalize_box(-50, -50, 100, 100, 1000, 1000);
    CVEDIX_ASSERT_EQ(negative.x, 0.0);
    CVEDIX_ASSERT_EQ(negative.y, 0.0);
    CVEDIX_ASSERT_TRUE(std::abs(negative.width - 0.05) < 1e-9);

    // Degenerate frame: no location rather than a division by zero.
    const auto empty = normalize_box(0, 0, 10, 10, 0, 0);
    CVEDIX_ASSERT_EQ(empty.width, 0.0);
    CVEDIX_ASSERT_EQ(empty.height, 0.0);
}

CVEDIX_TEST_CASE(timestamps_render_as_iso8601_utc) {
    CVEDIX_ASSERT_EQ(iso8601_utc(1700000000000), std::string("2023-11-14T22:13:20Z"));
    CVEDIX_ASSERT_EQ(iso8601_utc(0), std::string("1970-01-01T00:00:00Z"));
    CVEDIX_ASSERT_EQ(iso8601_utc(1785161453000), std::string("2026-07-27T14:10:53Z"));
}

CVEDIX_TEST_CASE(tracking_id_is_stable_per_camera_and_track) {
    const std::string first = stable_tracking_id("camera-01", 7);
    CVEDIX_ASSERT_EQ(first, stable_tracking_id("camera-01", 7));

    // A different track, and the same track on another camera, must not collide.
    CVEDIX_ASSERT_TRUE(first != stable_tracking_id("camera-01", 8));
    CVEDIX_ASSERT_TRUE(first != stable_tracking_id("camera-02", 7));

    // UUID-shaped so consumers that parse it as one keep working.
    CVEDIX_ASSERT_EQ(first.size(), 36u);
    CVEDIX_ASSERT_EQ(first[8], '-');
    CVEDIX_ASSERT_EQ(first[13], '-');
    CVEDIX_ASSERT_EQ(first[18], '-');
    CVEDIX_ASSERT_EQ(first[23], '-');
    CVEDIX_ASSERT_EQ(first[14], '5');   // version 5
    CVEDIX_ASSERT_TRUE(first[19] == '8' || first[19] == '9' ||
                       first[19] == 'a' || first[19] == 'b');   // variant 1
}

CVEDIX_TEST_CASE(event_item_carries_identity_location_and_status) {
    const auto input = sample_input(1);
    const auto payload = nlohmann::json::parse(build_heramind_ppe_payload(input));
    CVEDIX_ASSERT_TRUE(payload.is_array());

    const auto* event = find_item(payload, "event-ppe-violation");
    CVEDIX_ASSERT_TRUE(event != nullptr);
    CVEDIX_ASSERT_EQ((*event)["event_id"], input.event_id);
    CVEDIX_ASSERT_EQ((*event)["instance_id"], std::string("camera-01"));
    CVEDIX_ASSERT_EQ((*event)["ref_tracking_id"], stable_tracking_id("camera-01", 7));
    CVEDIX_ASSERT_EQ((*event)["object_class"], std::string("Person"));
    CVEDIX_ASSERT_EQ((*event)["event_timestamp_ms"], input.event_timestamp_ms);
    CVEDIX_ASSERT_EQ((*event)["system_timestamp"], input.system_timestamp_ms);
    CVEDIX_ASSERT_EQ((*event)["system_datetime"], std::string("2023-11-14T22:13:20Z"));
    CVEDIX_ASSERT_EQ((*event)["channel_index"], input.channel_index);
    CVEDIX_ASSERT_EQ((*event)["frame_index"], input.frame_index);
    CVEDIX_ASSERT_EQ((*event)["status"], std::string(ppe_status_label(1)));
    CVEDIX_ASSERT_EQ((*event)["missing_ppe_count"], 1);

    // Normalized location, not source pixels.
    const auto& location = (*event)["location"];
    CVEDIX_ASSERT_TRUE(std::abs(location["x"].get<double>() - 100.0 / 1280) < 1e-9);
    CVEDIX_ASSERT_TRUE(std::abs(location["width"].get<double>() - 320.0 / 1280) < 1e-9);
    CVEDIX_ASSERT_TRUE(location["x"].get<double>() <= 1.0);
    CVEDIX_ASSERT_TRUE(location["x"].get<double>() + location["width"].get<double>() <= 1.0 + 1e-9);

    // The original pixel box survives alongside the normalized one, so a
    // consumer can cut the crop itself without rescaling guesswork.
    CVEDIX_ASSERT_EQ((*event)["frame_width"], 1280);
    CVEDIX_ASSERT_EQ((*event)["frame_height"], 720);
}

CVEDIX_TEST_CASE(every_ppe_state_reports_the_right_attributes) {
    for (int mask = 0; mask < 4; ++mask) {
        const auto input = sample_input(mask);
        const auto payload = nlohmann::json::parse(build_heramind_ppe_payload(input));
        const auto* event = find_item(payload, "event-ppe-violation");
        CVEDIX_ASSERT_TRUE(event != nullptr);
        CVEDIX_ASSERT_EQ((*event)["has_helmet"], (mask & 1) == 0);
        CVEDIX_ASSERT_EQ((*event)["has_vest"], (mask & 2) == 0);
        CVEDIX_ASSERT_EQ((*event)["missing_ppe_count"], ((mask & 1) ? 1 : 0) + ((mask & 2) ? 1 : 0));

        // Booleans travel as "true"/"false" strings, the form the dashboard
        // transforms read; values not present must not be reported at all.
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "has_helmet", (mask & 1) == 0 ? "true" : "false"));
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "has_vest", (mask & 2) == 0 ? "true" : "false"));
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "missing_helmet", (mask & 1) ? "true" : "false"));
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "missing_vest", (mask & 2) ? "true" : "false"));
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "ppe_status", ppe_status_label(mask)));
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "track_id", 7));
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "frame_index", 300));
        CVEDIX_ASSERT_TRUE(has_attribute(payload, "person_confidence", input.confidence));
    }
}

CVEDIX_TEST_CASE(crop_item_round_trips_the_person_image) {
    const auto input = sample_input(3);
    const auto payload = nlohmann::json::parse(build_heramind_ppe_payload(input));

    const auto* crop = find_item(payload, "crop");
    CVEDIX_ASSERT_TRUE(crop != nullptr);
    CVEDIX_ASSERT_EQ((*crop)["ref_event_id"], input.event_id);
    CVEDIX_ASSERT_EQ((*crop)["ref_tracking_id"], stable_tracking_id("camera-01", 7));
    CVEDIX_ASSERT_EQ((*crop)["crop_timestamp_ms"], input.event_timestamp_ms);
    CVEDIX_ASSERT_EQ((*crop)["image_encoding"], std::string("base64"));
    CVEDIX_ASSERT_EQ((*crop)["image_format"], std::string("jpeg"));

    // Raw base64 — the form HeraMind's examples ingest, with no data: URI
    // prefix — and it must decode back to the exact bytes handed in.
    const auto image = (*crop)["image"].get<std::string>();
    CVEDIX_ASSERT_TRUE(image.rfind("data:", 0) != 0);
    const auto decoded = base64_decode(image);
    CVEDIX_ASSERT_EQ(decoded.size(), input.jpeg.size());
    CVEDIX_ASSERT_TRUE(std::equal(decoded.begin(), decoded.end(), input.jpeg.begin()));

    // A crop with no location would be unusable, so it repeats the person box.
    CVEDIX_ASSERT_TRUE((*crop).contains("location"));
    CVEDIX_ASSERT_TRUE(std::abs((*crop)["location"]["width"].get<double>() - 320.0 / 1280) < 1e-9);
}

CVEDIX_TEST_CASE(no_image_means_no_crop_item) {
    auto input = sample_input(0);
    input.jpeg.clear();
    const auto payload = nlohmann::json::parse(build_heramind_ppe_payload(input));
    CVEDIX_ASSERT_TRUE(find_item(payload, "crop") == nullptr);
    // The violation itself is still reported.
    CVEDIX_ASSERT_TRUE(find_item(payload, "event-ppe-violation") != nullptr);
}

CVEDIX_TEST_CASE(every_item_is_self_correlating) {
    const auto payload = nlohmann::json::parse(build_heramind_ppe_payload(sample_input(3)));
    const std::string tracking = stable_tracking_id("camera-01", 7);
    CVEDIX_ASSERT_TRUE(payload.size() >= 3u);
    for (const auto& item : payload) {
        // HeraMind correlates items by instance + tracking + timestamp, so each
        // one must carry the set even when the others are dropped in transit.
        CVEDIX_ASSERT_EQ(item["instance_id"], std::string("camera-01"));
        CVEDIX_ASSERT_EQ(item["ref_tracking_id"], tracking);
        CVEDIX_ASSERT_EQ(item["system_timestamp"], (int64_t)1700000000000);
        CVEDIX_ASSERT_EQ(item["system_datetime"], std::string("2023-11-14T22:13:20Z"));
        CVEDIX_ASSERT_TRUE(item.contains("event_timestamp_ms"));
        CVEDIX_ASSERT_TRUE(item.contains("$id"));
    }
}

int main(int argc, char** argv) { return cvedix_test::run_all(argc, argv); }
