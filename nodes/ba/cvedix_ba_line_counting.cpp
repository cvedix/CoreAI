#include "cvedix_ba_line_counting.h"

namespace cvedix_nodes
{
    cvedix_ba_line_counting::cvedix_ba_line_counting(std::string node_name,
                                        std::map<int, std::vector<cvedix_objects::cvedix_line>> all_line_settings,
                                        std::map<int, std::vector<cvedix_objects::cvedix_ba_direct_type>> all_line_detect_directions,
                                        bool need_record_image,
                                        bool need_record_video):
                                        cvedix_node(node_name), all_line_settings(all_line_settings), all_line_detect_directions(all_line_detect_directions), need_record_image(need_record_image), need_record_video(need_record_video) 
    {
        CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(), to_string().c_str()));    
        
        if (all_line_settings.size() != all_line_detect_directions.size()) {
            CVEDIX_ERROR(cvedix_utils::string_format("[%s] Line settings size and line directions size do not match!", node_name.c_str()));
            throw std::invalid_argument("Line settings size and line directions size do not match!");
        }

        for (auto it = all_line_settings.begin(); it != all_line_settings.end(); ++it) {
            int channel_id = it->first;
            if (all_line_settings[channel_id].size() != all_line_detect_directions[channel_id].size()) {
                CVEDIX_ERROR(cvedix_utils::string_format("[%s] Line settings and line directions size do not match for channel %d!", node_name.c_str(), channel_id));
                throw std::invalid_argument("Line settings and line directions size do not match for a channel!");
            }
            all_line_cross_counting[channel_id] = std::vector<int>(all_line_settings[channel_id].size(), 0);
        }

        this->initialized();
    }
    
    cvedix_ba_line_counting::~cvedix_ba_line_counting() 
    {
        deinitialized();
    }

    static float cross2d(const cvedix_objects::cvedix_point& a,
                     const cvedix_objects::cvedix_point& b,
                     const cvedix_objects::cvedix_point& c)
    {
        return (b.x - a.x) * (c.y - a.y)
            - (b.y - a.y) * (c.x - a.x);
    }

    static bool segments_intersect(
        const cvedix_objects::cvedix_point& p1,
        const cvedix_objects::cvedix_point& p2,
        const cvedix_objects::cvedix_point& q1,
        const cvedix_objects::cvedix_point& q2)
    {
        float d1 = cross2d(p1, p2, q1);
        float d2 = cross2d(p1, p2, q2);
        float d3 = cross2d(q1, q2, p1);
        float d4 = cross2d(q1, q2, p2);

        if (d1 * d2 < 0 && d3 * d4 < 0)
            return true;

        return false;
    }

    static bool is_point_crossing_line(
        const cvedix_objects::cvedix_point& prev_point,
        const cvedix_objects::cvedix_point& curr_point,
        const cvedix_objects::cvedix_line& line,
        const cvedix_objects::cvedix_ba_direct_type& direction)
    {
        const auto& A = line.start;
        const auto& B = line.end;

        // 1. Check segment intersection
        if (!segments_intersect(prev_point, curr_point, A, B))
            return false;

        // 2. Check moving direction
        float dx = curr_point.x - prev_point.x;
        float dy = curr_point.y - prev_point.y;

        switch (direction)
        {
            case cvedix_objects::cvedix_ba_direct_type::UP:
                return dy < 0;

            case cvedix_objects::cvedix_ba_direct_type::DOWN:
                return dy > 0;

            case cvedix_objects::cvedix_ba_direct_type::LEFT:
                return dx < 0;

            case cvedix_objects::cvedix_ba_direct_type::RIGHT:
                return dx > 0;

            default:
                return false;
        }
    }
    
    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ba_line_counting::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta)
    {
        if (all_line_settings.count(meta->channel_index) == 0) {
            return meta;
        }

        const auto& line_setting_list = all_line_settings.at(meta->channel_index);
        const auto& line_direction_list = all_line_detect_directions.at(meta->channel_index);
        auto& line_cross_counting_list = all_line_cross_counting.at(meta->channel_index);

        for (auto& target : meta->targets)
        {
            auto len = target->tracks.size();
            std::vector<int> involve_targets;

            if (len > 1 && target->track_id >=0)
            {
                auto curr_point = target->tracks[len - 1].track_point();
                auto prev_point = target->tracks[len - 2].track_point();

                for (size_t i = 0; i < line_setting_list.size(); i++)
                {
                    const auto& line = line_setting_list[i];
                    const auto& direction = line_direction_list[i];

                    if (is_point_crossing_line(prev_point, curr_point, line, direction))
                    {
                        line_cross_counting_list[i] += 1;
                        int total_count = line_cross_counting_list[i];
                        involve_targets.push_back(target->track_id);

                        std::string image_file_name_without_ext = "";    // empty means no recording image
                        std::string video_file_name_without_ext = "";    // empty means no recording video

                        // send image record control meta
                        if (need_record_image) {
                            image_file_name_without_ext = cvedix_utils::time_format(NOW, "line_counting_image__<year><mon><day><hour><min><sec><mili>");
                            auto image_record_control_meta = std::make_shared<cvedix_objects::cvedix_image_record_control_meta>(meta->channel_index, image_file_name_without_ext, true);
                            pendding_meta(image_record_control_meta);
                        }
                        // send video record control meta
                        if (need_record_video) {
                            video_file_name_without_ext = cvedix_utils::time_format(NOW, "line_counting_video__<year><mon><day><hour><min><sec><mili>");        
                            auto video_record_control_meta = std::make_shared<cvedix_objects::cvedix_video_record_control_meta>(meta->channel_index, video_file_name_without_ext);
                            pendding_meta(video_record_control_meta);
                        }

                        std::vector<cvedix_objects::cvedix_point> involve_region {line.start, line.end};
                        auto ba_result = std::make_shared<cvedix_objects::cvedix_ba_result>(cvedix_objects::cvedix_ba_type::CROSSLINE, 
                                                                                    meta->channel_index, 
                                                                                    meta->frame_index, 
                                                                                    involve_targets,
                                                                                    involve_region,
                                                                                    "crossline counting",
                                                                                    image_file_name_without_ext,
                                                                                    video_file_name_without_ext);
                        meta->ba_results.push_back(ba_result);

                        CVEDIX_INFO(cvedix_utils::string_format("[%s] [channel %d] target ID [%d] crossed line, total count for this line index [%d]: [%d]", node_name.c_str(), meta->channel_index, target->track_id, i, total_count));
                        if (need_record_image || need_record_video) {
                            CVEDIX_INFO(cvedix_utils::string_format("[%s] [channel %d] Recording triggered - Image: [%s], Video: [%s]", node_name.c_str(), meta->channel_index, image_file_name_without_ext.c_str(), video_file_name_without_ext.c_str()));
                        }
                    }
                }
            }
        }

        return meta;
    }    

} // namespace cvedix_nodes
