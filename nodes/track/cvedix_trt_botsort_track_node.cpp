#ifdef CVEDIX_WITH_TRT

#include "cvedix_trt_botsort_track_node.h"
#include "third_party/trt_botsort/DataType.h"
#include "third_party/trt_botsort/track.h"
#include <opencv2/core.hpp>

namespace cvedix_nodes {
    cvedix_botsort_track_node::cvedix_botsort_track_node(std::string node_name, 
        cvedix_track_for track_for,
        std::string tracker_config_path,
        std::string gmc_config_path,
        std::string reid_config_path,
        std::string reid_onnx_model_path
    ) : cvedix_track_node(node_name, track_for), tracker_config_path(tracker_config_path),
        gmc_config_path(gmc_config_path), reid_config_path(reid_config_path),
        reid_onnx_model_path(reid_onnx_model_path)
    {
            
        this->initialized();
    }

    cvedix_botsort_track_node::~cvedix_botsort_track_node() {
        deinitialized();
    }

    static float iou_tlwh(
    const cv::Rect2f& a,
    const cv::Rect2f& b)
    {
        float xx1 = std::max(a.x, b.x);
        float yy1 = std::max(a.y, b.y);
        float xx2 = std::min(a.x + a.width,  b.x + b.width);
        float yy2 = std::min(a.y + a.height, b.y + b.height);
    
        float w = std::max(0.f, xx2 - xx1);
        float h = std::max(0.f, yy2 - yy1);
        float inter = w * h;
        float uni = a.width * a.height + b.width * b.height - inter;
    
        return (uni > 0.f) ? inter / uni : 0.f;
    }

    void cvedix_botsort_track_node::track(int channel_index, 
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

        if (channel_trackers.count(channel_index) == 0) {
            channel_trackers[channel_index] = std::make_unique<BoTSORT>(tracker_config_path,
                                            gmc_config_path, reid_config_path,
                                            reid_onnx_model_path);
        }

        auto& tracker = channel_trackers[channel_index];
        
        std::vector<Detection> detections;
        for (int i = 0; i < target_rects.size(); i++) {
            cv::Rect_<float> bbox_tlwh(target_rects[i].x, target_rects[i].y, target_rects[i].width, target_rects[i].height);
            Detection det;
            det.confidence= 0.99f; // dummy confidence
            det.class_id = 0; // dummy class id
            det.bbox_tlwh = bbox_tlwh;
            detections.push_back(det);
        }

        std::vector<std::shared_ptr<Track>> tracks = tracker->track(detections, frame_meta->frame);

        for (size_t i = 0; i < target_rects.size(); ++i) {
            cv::Rect2f bbox_tlwh(target_rects[i].x, target_rects[i].y, target_rects[i].width, target_rects[i].height);

            float best_iou = 0.f;
            int best_j = -1;

            for (size_t j = 0; j < tracks.size(); ++j)
            {
                cv::Rect2f track_box(tracks[j]->det_tlwh[0],
                                     tracks[j]->det_tlwh[1],
                                     tracks[j]->det_tlwh[2],
                                     tracks[j]->det_tlwh[3]);
                float iou = iou_tlwh(bbox_tlwh, track_box);
                if (iou > best_iou)
                {
                    best_iou = iou;
                    best_j = j;
                }
            }

            if (best_j == -1){
                continue;
            }
            int track_id = tracks[best_j]->track_id;
            track_ids[i] = track_id;
        }
        
    }
}

#endif  // CVEDIX_WITH_TRT