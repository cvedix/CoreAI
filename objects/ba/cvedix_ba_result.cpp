#include "cvedix_ba_result.h"
#include "cvedix/objects/cvedix_frame_target.h"
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

namespace cvedix_objects {

    cvedix_ba_result::cvedix_ba_result(cvedix_ba_type type, 
                    int channel_index,
                    int frame_index,
                    std::vector<int> involve_target_ids_in_frame, 
                    std::vector<cvedix_objects::cvedix_point> involve_region_in_frame,
                    std::string ba_label,
                    std::string record_image_name,
                    std::string record_video_name):
                    type(type), channel_index(channel_index), frame_index(frame_index),
                    involve_target_ids_in_frame(involve_target_ids_in_frame),
                    involve_region_in_frame(involve_region_in_frame),
                    ba_label(ba_label),
                    record_image_name(record_image_name),
                    record_video_name(record_video_name) {
        
    }

    cvedix_ba_result::~cvedix_ba_result() {

    }

    std::string cvedix_ba_result::to_string() {
        std::stringstream ss;
        ss << "ba_result{type=" << ba_type_to_string(type)
           << ", channel=" << channel_index
           << ", frame=" << frame_index
           << ", label=" << ba_label
           << ", targets=" << involve_target_ids_in_frame.size()
           << ", details=" << involve_target_details.size();
        if (!event_id.empty()) {
            ss << ", event_id=" << event_id;
        }
        if (!region_type.empty()) {
            ss << ", region_type=" << region_type;
        }
        if (!region_name.empty()) {
            ss << ", region=" << region_name;
        }
        if (region_index >= 0) {
            ss << ", region_idx=" << region_index;
        }
        if (system_timestamp > 0) {
            ss << ", ts=" << static_cast<long long>(system_timestamp);
        }
        ss << "}";
        return ss.str();
    }

    std::shared_ptr<cvedix_ba_result> cvedix_ba_result::clone() {
        return std::make_shared<cvedix_ba_result>(*this);
    }

    void cvedix_ba_result::stamp_now() {
        auto now = std::chrono::system_clock::now();
        auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        system_timestamp = static_cast<double>(epoch_ms);
        event_timestamp_ms = system_timestamp;

        // Generate ISO 8601 datetime string
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        std::tm tm_val;
        gmtime_r(&time_t_val, &tm_val);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
        system_datetime = std::string(buf);

        // Auto-generate event_id if not set
        if (event_id.empty()) {
            event_id = generate_uuid();
        }
    }

    std::string cvedix_ba_result::generate_uuid() {
        static thread_local std::mt19937 rng(
            static_cast<unsigned>(
                std::chrono::steady_clock::now().time_since_epoch().count()
                ^ std::hash<std::thread::id>{}(std::this_thread::get_id())
            ));
        std::uniform_int_distribution<uint32_t> dist;

        uint32_t a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng);
        char uuid[37];
        std::snprintf(uuid, sizeof(uuid),
            "%08x-%04x-%04x-%04x-%04x%08x",
            a,
            (b >> 16) & 0xffff,
            ((b & 0x0fff) | 0x4000),           // version 4
            ((c >> 16) & 0x3fff) | 0x8000,     // variant 1
            c & 0xffff,
            d);
        return std::string(uuid);
    }

    void cvedix_ba_result::populate_target_details(
        const std::vector<std::shared_ptr<cvedix_frame_target>>& targets_in_frame,
        const cv::Mat& frame,
        bool include_crops) {
        
        involve_target_details.clear();
        involve_target_details.reserve(involve_target_ids_in_frame.size());

        int frame_w = frame.empty() ? 0 : frame.cols;
        int frame_h = frame.empty() ? 0 : frame.rows;

        for (int target_id : involve_target_ids_in_frame) {
            involved_target_info info;
            info.track_id = target_id;

            // Find matching target in frame
            auto it = std::find_if(targets_in_frame.begin(), targets_in_frame.end(),
                [target_id](const std::shared_ptr<cvedix_frame_target>& t) {
                    return t && t->track_id == target_id;
                });

            if (it != targets_in_frame.end()) {
                auto& target = *it;
                // Pixel coordinates
                info.x = target->x;
                info.y = target->y;
                info.width = target->width;
                info.height = target->height;

                // Normalized location (0.0-1.0)
                if (frame_w > 0 && frame_h > 0) {
                    info.location_x = static_cast<double>(info.x) / frame_w;
                    info.location_y = static_cast<double>(info.y) / frame_h;
                    info.location_w = static_cast<double>(info.width) / frame_w;
                    info.location_h = static_cast<double>(info.height) / frame_h;
                }

                info.class_id = target->primary_class_id;
                info.score = target->primary_score;
                info.object_class = target->primary_label;

                // Generate ref_tracking_id as UUID-like from track_id
                info.ref_tracking_id = generate_uuid();

                // Optionally crop the target from frame
                if (include_crops && !frame.empty()) {
                    int crop_x = std::max(0, info.x);
                    int crop_y = std::max(0, info.y);
                    int crop_w = std::min(info.width, frame.cols - crop_x);
                    int crop_h = std::min(info.height, frame.rows - crop_y);

                    if (crop_w > 0 && crop_h > 0) {
                        cv::Rect roi(crop_x, crop_y, crop_w, crop_h);
                        info.crop = frame(roi).clone();
                    }
                }
            }

            involve_target_details.push_back(std::move(info));
        }
    }

    std::string cvedix_ba_result::ba_type_to_string(cvedix_ba_type t) {
        switch (t) {
            case cvedix_ba_type::NONE: return "none";
            case cvedix_ba_type::CROSSLINE: return "crossline";
            case cvedix_ba_type::STOP: return "stop";
            case cvedix_ba_type::UNSTOP: return "unstop";
            case cvedix_ba_type::JAM: return "jam";
            case cvedix_ba_type::UNJAM: return "unjam";
            case cvedix_ba_type::AREA_ENTER: return "area_enter";
            case cvedix_ba_type::AREA_EXIT: return "area_exit";
            case cvedix_ba_type::SPEED: return "speed";
            case cvedix_ba_type::DIRECTION: return "direction";
            case cvedix_ba_type::DWELL: return "dwell";
            case cvedix_ba_type::QUEUE: return "queue";
            case cvedix_ba_type::FALL: return "fall";
            case cvedix_ba_type::CROWDING: return "crowding";
            case cvedix_ba_type::LOITERING: return "loitering";
            case cvedix_ba_type::FIGHT: return "fight";
            case cvedix_ba_type::PARKING: return "parking";
            case cvedix_ba_type::RED_LIGHT: return "red_light";
            case cvedix_ba_type::ILLEGAL_TURN: return "illegal_turn";
            case cvedix_ba_type::WRONG_WAY: return "wrong_way";
            case cvedix_ba_type::LANE_VIOLATION: return "lane_violation";
            case cvedix_ba_type::STOP_LINE: return "stop_line";
            case cvedix_ba_type::NO_ENTRY: return "no_entry";
            case cvedix_ba_type::ILLEGAL_UTURN: return "illegal_uturn";
            case cvedix_ba_type::HELMET: return "helmet";
            case cvedix_ba_type::INTRUSION_START: return "intrusion_start";
            case cvedix_ba_type::INTRUSION_END: return "intrusion_end";
            case cvedix_ba_type::LOITERING_END: return "loitering_end";
            default: return "unknown";
        }
    }
}