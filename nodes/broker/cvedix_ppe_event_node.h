#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include <atomic>
#include <functional>
#include <map>

namespace cvedix_nodes {

/// Wire format for a confirmed violation.
enum class ppe_payload_format {
    flat,     ///< Single self-contained JSON object (generic consumers).
    heramind  ///< HeraMind typed item array; see cvedix_heramind_event_payload.h.
};

struct ppe_event_config {
    std::string camera_id;
    std::string run_id;
    std::string archive_dir;
    double source_fps = 15;
    int confirm_frames = 3;
    int cooldown_ms = 10000;
    int person_class = 100;
    ppe_payload_format format = ppe_payload_format::flat;
};

// Place after PPE assessment. Produces one JSON message per confirmed unsafe
// person; the publisher returns true if queued for delivery (not broker ACK).
// Archives JSON/JPEG before sending. Callback/encoding errors are counted and
// reported without dropping the video frame; the application must check errors.
class cvedix_ppe_event_node : public cvedix_node {
    struct track_state {
        int last_frame = -1, mask = -1, consecutive = 0, last_event = -1;
    };
    const ppe_event_config config;
    const std::function<bool(const std::string&)> publisher;
    std::map<std::pair<int, int>, track_state> states;
    std::atomic<size_t> generated{0}, accepted{0}, errors{0}, skipped_untracked{0};
protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
public:
    cvedix_ppe_event_node(std::string name, ppe_event_config config,
                         std::function<bool(const std::string&)> publisher);
    ~cvedix_ppe_event_node() override;
    size_t generated_count() const { return generated.load(); }
    size_t accepted_count() const { return accepted.load(); }
    size_t error_count() const { return errors.load(); }
    size_t skipped_untracked_count() const { return skipped_untracked.load(); }
};
} // namespace cvedix_nodes
