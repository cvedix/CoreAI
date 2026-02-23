#include "cvedix_ocsort_track_node.h"
#include <Eigen/Dense>
#include <iostream>
#include <vector>

namespace cvedix_nodes {

    Eigen::Matrix<float, Eigen::Dynamic, 6> Vector2Matrix(std::vector<std::vector<float>> data) {
        Eigen::Matrix<float, Eigen::Dynamic, 6> matrix(data.size(), data[0].size());
        for (int i = 0; i < data.size(); ++i) {
            for (int j = 0; j < data[0].size(); ++j) {
                matrix(i, j) = data[i][j];
            }
        }
        return matrix;
    }

    cvedix_ocsort_track_node::cvedix_ocsort_track_node(std::string node_name, 
                        cvedix_track_for track_for,
                        float det_thresh,
                        int   max_age,
                        int   min_hits,
                        float iou_threshold,
                        int   delta_t,
                        std::string asso_func,
                        float inertia,
                        bool  use_byte):
                        cvedix_track_node(node_name, track_for),
                        det_thresh(det_thresh),
                        max_age(max_age),
                        min_hits(min_hits),
                        iou_threshold(iou_threshold),
                        delta_t(delta_t),
                        asso_func(asso_func),
                        inertia(inertia),
                        use_byte(use_byte)
    {
        this->initialized();
    }

    cvedix_ocsort_track_node::~cvedix_ocsort_track_node()
    {
        deinitialized();
    }

    static size_t hash_vector_float(const std::vector<float>& v) noexcept {
        size_t h = v.size();
        for (float x : v)
            h = h * 131 + static_cast<int>(x * 1000);
        return h;
    }

    void cvedix_ocsort_track_node::track(int channel_index, 
					const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta,
					const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                    const std::vector<std::vector<float>>& target_embeddings, 
                    std::vector<int>& track_ids)
    {
        track_ids.resize(target_rects.size());
        for (auto&  item : track_ids) {
			item = -1;
		}

        if (channel_index < 0) {
            return;
        }

        // Init tracker for a channel
        if (channel_trackers.find(channel_index) == channel_trackers.end())
        {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Init tracker for channel %d", node_name.c_str(), channel_index));
            channel_trackers.emplace(
                channel_index,
                ocsort::OCSort(det_thresh, max_age, min_hits, iou_threshold, delta_t, asso_func, inertia, use_byte)
            );
        }

        std::vector<std::vector<float>> data;
        std::unordered_map<size_t, int> target_map;

        for (int i = 0; i < target_rects.size(); i++)
        {
            std::vector<float> row;
            row.push_back(target_rects[i].x);
            row.push_back(target_rects[i].y);
            row.push_back(target_rects[i].x + target_rects[i].width);
            row.push_back(target_rects[i].y + target_rects[i].height);
            row.push_back(1); // confidence (current unused)
            row.push_back(0); // classid or label (current unused)

            data.push_back(row);

            target_map.emplace(
                hash_vector_float({row[0], row[1], row[2], row[3]}),
                i
            );

        }

        auto it = channel_trackers.find(channel_index);
        
        if (!data.empty() && it != channel_trackers.end()) {
            auto& tracker = it->second;
            std::vector<Eigen::RowVectorXf> res = tracker.update(Vector2Matrix(data));

            for (size_t i = 0; i < res.size(); i++) {
                const auto& j = res[i];
                int ID = static_cast<int>(j[4]);
                auto it = target_map.find(hash_vector_float({j[0], j[1], j[2], j[3]}));
                if (it != target_map.end())
                    track_ids[it->second] = ID;
                // int Class = int(j[5]);
                // float conf = j[6];


            }
                
            data.clear();
        }
    }

} //namespace cvedix_nodes