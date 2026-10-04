#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <opencv2/core.hpp>

namespace cvedix_nodes {

struct ppe_observation {
    cv::Rect2f box;
    int class_id;
};

struct uniform_assessment {
    int person_index;
    int shirt_index = -1;
    int pants_index = -1;
    int missing_mask() const { return (shirt_index < 0 ? 1 : 0) | (pants_index < 0 ? 2 : 0); }
};

struct ppe_assessment {
    int person_index;
    int helmet_index = -1;
    int vest_index = -1;
    // 0: complete, 1: missing helmet, 2: missing vest, 3: missing both.
    int missing_mask() const { return (helmet_index < 0 ? 1 : 0) | (vest_index < 0 ? 2 : 0); }
};

// All observations must come from the same frame/channel. Match each PPE item
// to at most one person, using containment and head/torso position. These are
// per-frame detection associations, not proof that the equipment is worn.
inline std::vector<ppe_assessment> associate_ppe(
    const std::vector<ppe_observation>& observations,
    int person_class = 100, int helmet_class = 0, int vest_class = 1) {
    std::vector<ppe_assessment> result;
    for (size_t i = 0; i < observations.size(); ++i) {
        const auto& o = observations[i];
        if (o.class_id == person_class && o.box.width > 0 && o.box.height > 0)
            result.push_back({static_cast<int>(i)});
    }
    struct candidate { size_t person; int item; float cost; };
    for (const bool helmet : {true, false}) {
        std::vector<candidate> candidates;
        for (size_t p = 0; p < result.size(); ++p) {
            const auto& person = observations[result[p].person_index].box;
            // A helmet can extend just beyond the top/sides of a person box.
            const cv::Rect2f region(person.x - .08f * person.width,
                person.y - .10f * person.height, 1.16f * person.width, 1.10f * person.height);
            for (size_t i = 0; i < observations.size(); ++i) {
                const auto& item = observations[i];
                if (item.class_id != (helmet ? helmet_class : vest_class) ||
                    item.box.width <= 0 || item.box.height <= 0) continue;
                const float nx = (item.box.x + item.box.width * .5f - person.x) / person.width;
                const float ny = (item.box.y + item.box.height * .5f - person.y) / person.height;
                const float coverage = (region & item.box).area() / item.box.area();
                if (coverage < .6f || nx < -.08f || nx > 1.08f ||
                    ny < (helmet ? -.10f : .15f) || ny > (helmet ? .45f : .80f)) continue;
                const float cost = std::abs(nx - .5f) + std::abs(ny - (helmet ? .12f : .45f))
                                   + (1.f - coverage);
                candidates.push_back({p, static_cast<int>(i), cost});
            }
        }
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) { return a.cost < b.cost; });
        std::vector<bool> used(observations.size(), false);
        for (const auto& c : candidates) {
            auto& assigned = helmet ? result[c.person].helmet_index : result[c.person].vest_index;
            if (assigned < 0 && !used[c.item]) {
                assigned = c.item;
                used[c.item] = true;
            }
        }
    }
    return result;
}

// Match shirt and pants detections to each person using upper/lower body zones.
inline std::vector<uniform_assessment> associate_uniform(
    const std::vector<ppe_observation>& observations,
    int person_class = 100, int shirt_class = 0, int pants_class = 1) {
    std::vector<uniform_assessment> result;
    for (size_t i = 0; i < observations.size(); ++i) {
        const auto& o = observations[i];
        if (o.class_id == person_class && o.box.width > 0 && o.box.height > 0)
            result.push_back({static_cast<int>(i)});
    }
    struct candidate { size_t person; int item; float cost; };
    for (const bool shirt : {true, false}) {
        std::vector<candidate> candidates;
        for (size_t p = 0; p < result.size(); ++p) {
            const auto& person = observations[result[p].person_index].box;
            const cv::Rect2f region(person.x - .08f * person.width,
                person.y - .04f * person.height, 1.16f * person.width, 1.08f * person.height);
            for (size_t i = 0; i < observations.size(); ++i) {
                const auto& item = observations[i];
                if (item.class_id != (shirt ? shirt_class : pants_class) ||
                    item.box.width <= 0 || item.box.height <= 0) continue;
                const float nx = (item.box.x + item.box.width * .5f - person.x) / person.width;
                const float ny = (item.box.y + item.box.height * .5f - person.y) / person.height;
                const float coverage = (region & item.box).area() / item.box.area();
                const float min_y = shirt ? .10f : .35f;
                const float max_y = shirt ? .65f : 1.02f;
                const float target_y = shirt ? .35f : .72f;
                if (coverage < .6f || nx < -.08f || nx > 1.08f || ny < min_y || ny > max_y) continue;
                const float cost = std::abs(nx - .5f) + std::abs(ny - target_y) + (1.f - coverage);
                candidates.push_back({p, static_cast<int>(i), cost});
            }
        }
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) { return a.cost < b.cost; });
        std::vector<bool> used(observations.size(), false);
        for (const auto& c : candidates) {
            auto& assigned = shirt ? result[c.person].shirt_index : result[c.person].pants_index;
            if (assigned < 0 && !used[c.item]) {
                assigned = c.item;
                used[c.item] = true;
            }
        }
    }
    return result;
}

inline const char* uniform_status_label(int mask) {
    switch (mask) {
        case 0: return "uniform:ok";
        case 1: return "uniform:missing_shirt";
        case 2: return "uniform:missing_pants";
        default: return "uniform:missing_shirt_and_pants";
    }
}

inline const char* ppe_status_label(int mask) {
    switch (mask) {
        case 0: return "ppe:ok";
        case 1: return "ppe:missing_helmet";
        case 2: return "ppe:missing_vest";
        default: return "ppe:missing_helmet_and_vest";
    }
}

} // namespace cvedix_nodes
