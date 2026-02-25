#include "cvedix_ba_line_counting_node.h"

namespace cvedix_nodes
{
    cvedix_ba_line_counting_node::cvedix_ba_line_counting_node(std::string node_name,
                                        std::map<int, std::vector<cvedix_nodes::cvedix_ba_line_couting_setting>> all_line_settings,
                                        bool need_record_image,
                                        bool need_record_video):
                                        cvedix_node(node_name), all_line_settings(all_line_settings), need_record_image(need_record_image), need_record_video(need_record_video) 
    {
        CVEDIX_INFO(cvedix_utils::string_format("[%s] %s", node_name.c_str(), to_string().c_str()));    

        for (auto it = all_line_settings.begin(); it != all_line_settings.end(); ++it) {
            int channel_id = it->first;
            all_line_cross_counting[channel_id] = std::vector<int>(all_line_settings[channel_id].size(), 0);
        }

        this->initialized();
    }
    
    cvedix_ba_line_counting_node::~cvedix_ba_line_counting_node() 
    {
        deinitialized();
    }

    static float cross2d(const cvedix_objects::cvedix_point& a,
                     const cvedix_objects::cvedix_point& b,
                     const cvedix_objects::cvedix_point& c)
    {
        return (b.y - a.y) * (c.x - a.x) - (b.x - a.x) * (c.y - a.y);
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

    static bool check_direction(
        const cvedix_objects::cvedix_point& p1,
        const cvedix_objects::cvedix_point& p2,
        const cvedix_objects::cvedix_point& q1,
        const cvedix_objects::cvedix_point& q2,
        const cvedix_objects::cvedix_ba_direct_type& direction)
    {
        float d3 = cross2d(q1, q2, p1);
        float d4 = cross2d(q1, q2, p2);

        switch (direction)
        {
            case cvedix_objects::cvedix_ba_direct_type::IN:
                return d3 > 0 && d4 < 0;
            case cvedix_objects::cvedix_ba_direct_type::OUT:
                return d3 < 0 && d4 > 0;
            case cvedix_objects::cvedix_ba_direct_type::BOTH:
                return d3 * d4 < 0;
            default:
                return false;
        }

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
        return check_direction(prev_point, curr_point, A, B, direction);
    }
    
    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_ba_line_counting_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta)
    {
        if (all_line_settings.count(meta->channel_index) == 0) {
            return meta;
        }

        const auto& line_setting_list = all_line_settings.at(meta->channel_index);
        auto& line_cross_counting_list = all_line_cross_counting.at(meta->channel_index);

        for (auto& target : meta->targets)
        {
            auto len = target->tracks.size();
            std::vector<int> involve_targets;

            if (len > 1 && target->track_id >=0)
            {
                auto curr_point = target->tracks[len - 1].track_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);
                auto prev_point = target->tracks[len - 2].track_point(cvedix_objects::cvedix_rect_anchor_point::MID_BOTTOM);

                for (size_t i = 0; i < line_setting_list.size(); i++)
                {
                    const auto& line = line_setting_list[i].line;
                    const auto& direction = line_setting_list[i].direction;
                    const auto& setting_name = line_setting_list[i].setting_name;

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
                                                                                    setting_name,
                                                                                    image_file_name_without_ext,
                                                                                    video_file_name_without_ext);
                        meta->ba_results.push_back(ba_result);

                        CVEDIX_INFO(cvedix_utils::string_format("[%s] [channel %d] target ID [%d] crossed line, total count for this line [%s]: [%d]", node_name.c_str(), meta->channel_index, target->track_id, setting_name.c_str(), total_count));
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
