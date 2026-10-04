#include "cvedix_ba_ppe_safety_node.h"
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <iomanip>
#include <sstream>

namespace cvedix_nodes {

cvedix_ba_ppe_safety_node::cvedix_ba_ppe_safety_node(
    std::string name, int person, int helmet, int vest, bool uniform)
    : cvedix_node(name), person_class(person), helmet_class(helmet), vest_class(vest),
      uniform_mode(uniform) {
    if (person == helmet || person == vest || helmet == vest)
        throw std::invalid_argument("Person, helmet and vest classes must be distinct");
    initialized();
}

cvedix_ba_ppe_safety_node::~cvedix_ba_ppe_safety_node() { deinitialized(); }

std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ba_ppe_safety_node::handle_frame_meta(
    std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
    std::vector<ppe_observation> observations;
    for (const auto& t : meta->targets) {
        observations.push_back({cv::Rect2f(t->x, t->y, t->width, t->height),
            t->frame_index == meta->frame_index && t->channel_index == meta->channel_index
                ? t->primary_class_id : -1});
    }
    struct assessment {
        int person_index;
        int first_index;
        int second_index;
        int mask;
    };
    std::vector<assessment> assessments;
    if (uniform_mode) {
        for (const auto& a : associate_uniform(observations, person_class, helmet_class, vest_class))
            assessments.push_back({a.person_index, a.shirt_index, a.pants_index, a.missing_mask()});
    } else {
        for (const auto& a : associate_ppe(observations, person_class, helmet_class, vest_class))
            assessments.push_back({a.person_index, a.helmet_index, a.vest_index, a.missing_mask()});
    }

    if (meta->osd_frame.empty()) meta->osd_frame = meta->frame.clone();
    int safe = 0, unsafe = 0;
    const double font = std::max(.5, meta->frame.cols / 2200.0);
    const int thickness = std::max(2, meta->frame.cols / 900);
    std::vector<int> owners(meta->targets.size(), -1);
    for (const auto& a : assessments) {
        auto& t = meta->targets[a.person_index];
        if (a.first_index >= 0) owners[a.first_index] = t->track_id;
        if (a.second_index >= 0) owners[a.second_index] = t->track_id;
        t->secondary_class_ids.push_back(a.mask);
        t->secondary_labels.push_back(uniform_mode
            ? uniform_status_label(a.mask) : ppe_status_label(a.mask));
        t->secondary_scores.push_back(t->primary_score);
        a.mask ? ++unsafe : ++safe;
        if (meta->osd_frame.empty()) continue;

        const cv::Scalar color = a.mask ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 220, 0);
        const char* status = uniform_mode
            ? (a.mask == 0 ? "DONG PHUC DAY DU" : a.mask == 1 ? "THIEU AO" :
               a.mask == 2 ? "THIEU QUAN" : "THIEU AO + QUAN")
            : (a.mask == 0 ? "AN TOAN" : a.mask == 1 ? "LOI: THIEU MU" :
               a.mask == 2 ? "LOI: THIEU AO" : "LOI: THIEU MU + AO");
        const std::string id = t->track_id >= 0 ? "#" + std::to_string(t->track_id) : "?";
        const std::string text = "Nguoi " + id + " - " + status;
        for (size_t j = 1; j < t->tracks.size(); ++j) {
            const auto& prev = t->tracks[j - 1];
            const auto& curr = t->tracks[j];
            cv::line(meta->osd_frame, cv::Point(prev.x + prev.width / 2, prev.y + prev.height),
                cv::Point(curr.x + curr.width / 2, curr.y + curr.height), color, 2, cv::LINE_AA);
        }
        cv::rectangle(meta->osd_frame, cv::Rect(t->x, t->y, t->width, t->height), color, thickness);
        int baseline = 0;
        const auto size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, font, thickness, &baseline);
        const int x = std::max(0, std::min(t->x, meta->osd_frame.cols - size.width - 8));
        const int y = std::max(size.height + 8, t->y - 8);
        cv::rectangle(meta->osd_frame, cv::Rect(x, y - size.height - 5,
                      size.width + 8, size.height + baseline + 10), cv::Scalar(20, 20, 20), cv::FILLED);
        cv::putText(meta->osd_frame, text, cv::Point(x + 4, y),
                    cv::FONT_HERSHEY_SIMPLEX, font, color, thickness, cv::LINE_AA);
    }

    for (size_t i = 0; i < meta->targets.size() && !meta->osd_frame.empty(); ++i) {
        const auto& t = meta->targets[i];
        if (t->frame_index != meta->frame_index || t->channel_index != meta->channel_index ||
            (t->primary_class_id != helmet_class && t->primary_class_id != vest_class)) continue;
        const bool first_item = t->primary_class_id == helmet_class;
        const cv::Scalar item_color = first_item ? cv::Scalar(0, 220, 255) : cv::Scalar(255, 200, 0);
        cv::rectangle(meta->osd_frame, cv::Rect(t->x, t->y, t->width, t->height),
                      item_color, std::max(2, thickness - 1));
        std::ostringstream label;
        label << (uniform_mode ? (first_item ? "Ao" : "Quan") : (first_item ? "Mu" : "Vest"));
        if (owners[i] >= 0) label << " #" << owners[i];
        label << ' ' << std::fixed << std::setprecision(2) << t->primary_score;
        const double gear_font = std::max(.45, font * .65);
        int baseline = 0;
        const auto size = cv::getTextSize(label.str(), cv::FONT_HERSHEY_SIMPLEX, gear_font, 2, &baseline);
        const int x = std::max(0, std::min(t->x + 3, meta->osd_frame.cols - size.width - 6));
        const int y = std::min(meta->osd_frame.rows - baseline - 5, t->y + size.height + 6);
        cv::rectangle(meta->osd_frame, cv::Rect(x, y - size.height - 3,
                      size.width + 6, size.height + baseline + 6), cv::Scalar(20, 20, 20), cv::FILLED);
        cv::putText(meta->osd_frame, label.str(), cv::Point(x + 3, y),
                    cv::FONT_HERSHEY_SIMPLEX, gear_font, item_color, 2, cv::LINE_AA);
    }

    if (!meta->osd_frame.empty()) {
        const std::string summary = uniform_mode
            ? "DONG PHUC | DAT: " + std::to_string(safe) + " | THIEU: " + std::to_string(unsafe)
            : "PPE | AN TOAN: " + std::to_string(safe) + " | LOI: " + std::to_string(unsafe);
        cv::putText(meta->osd_frame, summary, cv::Point(20, 45),
                    cv::FONT_HERSHEY_SIMPLEX, font, cv::Scalar(0, 0, 0), thickness + 4, cv::LINE_AA);
        cv::putText(meta->osd_frame, summary, cv::Point(20, 45),
                    cv::FONT_HERSHEY_SIMPLEX, font, cv::Scalar(255, 255, 255), thickness, cv::LINE_AA);
    }
    return meta;
}

} // namespace cvedix_nodes