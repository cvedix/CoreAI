#pragma once

#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix_ppe_association.h"

namespace cvedix_nodes {

// Attach after merging the person and PPE branches for the same frame.
// Appends a secondary classification (mask 0..3 and ppe:* label) to each
// person and draws green/red person boxes, track IDs/trails, yellow helmets
// and cyan vests on osd_frame. Gear labels use their owner's ID when assigned.
// Does not carry safety status between frames.
class cvedix_ba_ppe_safety_node : public cvedix_node {
    int person_class, helmet_class, vest_class;
    bool uniform_mode;
protected:
    std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
        std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
public:
    cvedix_ba_ppe_safety_node(std::string name, int person_class = 100,
                            int helmet_class = 0, int vest_class = 1,
                            bool uniform_mode = false);
    ~cvedix_ba_ppe_safety_node() override;
};

} // namespace cvedix_nodes
