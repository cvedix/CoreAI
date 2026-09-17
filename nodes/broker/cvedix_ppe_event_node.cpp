#include "cvedix_ppe_event_node.h"
#include "cvedix/nodes/ba/cvedix_ppe_association.h"
#include "cvedix/nodes/broker/cvedix_heramind_event_payload.h"
#include "cvedix/third_party/nlohmann/json.hpp"
#include "cvedix/third_party/cpp_base64/base64.h"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace cvedix_nodes {
namespace {
void save_bytes(const std::filesystem::path& path, const char* data, size_t size) {
    std::ofstream stream;
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream.open(path, std::ios::binary);
    stream.write(data, size);
    stream.close();
}

/// Flat, self-contained payload: one object holding every field.
std::string build_flat_ppe_payload(const ppe_event_payload_input& in,
                                   const std::string& run_id, const cv::Rect& bbox) {
    nlohmann::json missing = nlohmann::json::array();
    if (in.mask & 1) missing.push_back("helmet");
    if (in.mask & 2) missing.push_back("vest");
    nlohmann::json event = {
        {"schema_version", "1.0"}, {"event_type", "ppe_violation"},
        {"event_id", in.event_id}, {"camera_id", in.camera_id}, {"run_id", run_id},
        {"tracking_id", in.track_id}, {"channel_index", in.channel_index},
        {"frame_index", in.frame_index}, {"system_timestamp_ms", in.system_timestamp_ms},
        {"source_timestamp_ms", static_cast<double>(in.event_timestamp_ms)},
        {"source_timestamp_basis", "frame_index/source_fps"},
        {"object_class", "person"}, {"confidence", in.confidence},
        {"bbox", {{"x", bbox.x}, {"y", bbox.y}, {"width", bbox.width}, {"height", bbox.height}}},
        {"bbox_format", "xywh"}, {"bbox_coordinate_space", "source_pixels"},
        {"frame_width", in.frame_width}, {"frame_height", in.frame_height},
        {"has_helmet", (in.mask & 1) == 0}, {"has_vest", (in.mask & 2) == 0},
        {"missing_ppe", missing}, {"status", ppe_status_label(in.mask)},
        {"crop_image", base64_encode(in.jpeg.data(), in.jpeg.size())},
        {"crop_image_encoding", "base64"}, {"crop_image_format", "jpeg"},
        {"crop_width", bbox.width}, {"crop_height", bbox.height}
    };
    return event.dump();
}
}

cvedix_ppe_event_node::cvedix_ppe_event_node(std::string name, ppe_event_config cfg,
    std::function<bool(const std::string&)> pub)
    : cvedix_node(name), config(std::move(cfg)), publisher(std::move(pub)) {
    if (config.camera_id.empty() || config.run_id.empty() || !publisher ||
        !std::isfinite(config.source_fps) || config.source_fps <= 0 ||
        config.confirm_frames < 1 || config.cooldown_ms < 0)
        throw std::invalid_argument("Invalid PPE event configuration");
    if (!config.archive_dir.empty()) std::filesystem::create_directories(config.archive_dir);
    initialized();
}

cvedix_ppe_event_node::~cvedix_ppe_event_node() { deinitialized(); }

std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ppe_event_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    const int cooldown_frames = std::max(1, static_cast<int>(std::ceil(config.cooldown_ms * config.source_fps / 1000.0)));
    for (const auto& t : meta->targets) {
        if (t->primary_class_id != config.person_class || t->frame_index != meta->frame_index ||
            t->channel_index != meta->channel_index) continue;
        int mask = -1;
        for (size_t i = 0; i < t->secondary_labels.size() && i < t->secondary_class_ids.size(); ++i) {
            const int candidate = t->secondary_class_ids[i];
            if (candidate >= 0 && candidate <= 3 && t->secondary_labels[i] == ppe_status_label(candidate)) mask = candidate;
        }
        if (mask < 0) continue;
        if (t->track_id < 0) { if (mask) ++skipped_untracked; continue; }
        auto& state = states[{meta->channel_index, t->track_id}];
        // Ignore a duplicate/out-of-order frame instead of advancing confirmation.
        if (meta->frame_index <= state.last_frame) continue;
        state.consecutive = mask && mask == state.mask && meta->frame_index == state.last_frame + 1
            ? state.consecutive + 1 : (mask ? 1 : 0);
        state.mask = mask;
        state.last_frame = meta->frame_index;
        if (!mask || state.consecutive < config.confirm_frames ||
            (state.last_event >= 0 && meta->frame_index - state.last_event < cooldown_frames)) continue;
        state.last_event = meta->frame_index;
        try {
            const cv::Rect bbox = cv::Rect(t->x, t->y, t->width, t->height) &
                                  cv::Rect(0, 0, meta->frame.cols, meta->frame.rows);
            if (bbox.empty()) throw std::runtime_error("Empty person crop");
            std::vector<uchar> jpeg;
            if (!cv::imencode(".jpg", meta->frame(bbox), jpeg, {cv::IMWRITE_JPEG_QUALITY, 85}))
                throw std::runtime_error("Cannot encode event crop");
            const auto event_id = cvedix_objects::cvedix_ba_result::generate_uuid();
            const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const auto event_timestamp_ms = static_cast<int64_t>(std::llround(
                meta->frame_index * 1000.0 / config.source_fps));
            const ppe_event_payload_input input{
                config.camera_id, event_id, t->track_id, mask,
                meta->channel_index, meta->frame_index,
                meta->frame.cols, meta->frame.rows,
                bbox.x, bbox.y, bbox.width, bbox.height,
                t->primary_score, now_ms, event_timestamp_ms, "Person", std::move(jpeg)};
            const std::string payload = config.format == ppe_payload_format::heramind
                ? build_heramind_ppe_payload(input)
                : build_flat_ppe_payload(input, config.run_id, bbox);
            if (!config.archive_dir.empty()) {
                const auto root = std::filesystem::path(config.archive_dir);
                save_bytes(root / (event_id + ".jpg"),
                    reinterpret_cast<const char*>(input.jpeg.data()), input.jpeg.size());
                save_bytes(root / (event_id + ".json"), payload.data(), payload.size());
            }
            ++generated;
            if (publisher(payload)) ++accepted;
            else {
                ++errors;
                CVEDIX_ERROR("[" + node_name + "] MQTT event was not queued; inspect the event archive");
            }
        } catch (const std::exception& e) {
            ++errors;
            CVEDIX_ERROR("[" + node_name + "] PPE event failed: " + e.what());
        } catch (...) {
            ++errors;
            CVEDIX_ERROR("[" + node_name + "] PPE event publisher failed");
        }
    }
    // Bound inactive track state while retaining per-track cooldown history.
    const double retention = std::max(60.0 * config.source_fps, 2.0 * cooldown_frames);
    for (auto it = states.begin(); it != states.end();) {
        if (it->first.first == meta->channel_index && meta->frame_index - it->second.last_frame > retention)
            it = states.erase(it);
        else ++it;
    }
    return meta;
}
} // namespace cvedix_nodes
