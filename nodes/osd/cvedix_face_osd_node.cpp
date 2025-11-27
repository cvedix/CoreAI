
#include <opencv2/imgproc.hpp>
#include <cmath>
#include "cvedix_face_osd_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"

namespace cvedix_nodes {
        
    cvedix_face_osd_node::cvedix_face_osd_node(std::string node_name): cvedix_node(node_name) {
        this->initialized();
    }
    
    cvedix_face_osd_node::~cvedix_face_osd_node() {
        deinitialized();
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_face_osd_node::handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) {
        // operations on osd_frame
        if (meta->osd_frame.empty()) {
            meta->osd_frame = meta->frame.clone();
        }
        auto& canvas = meta->osd_frame;
        
        // Debug: log number of face targets
        if (meta->face_targets.size() > 0) {
            CVEDIX_INFO(cvedix_utils::string_format("[%s] Drawing %zu face targets on frame %llu (canvas size: %dx%d)",
                                                     node_name.c_str(),
                                                     meta->face_targets.size(),
                                                     meta->frame_index,
                                                     canvas.cols, canvas.rows));
        }
        
        // scan face targets
        for(auto& i : meta->face_targets) {
            // Validate and clamp coordinates before drawing
            int x = std::max(0, std::min(i->x, canvas.cols - 1));
            int y = std::max(0, std::min(i->y, canvas.rows - 1));
            int width = std::max(1, std::min(i->width, canvas.cols - x));
            int height = std::max(1, std::min(i->height, canvas.rows - y));
            
            // Additional validation: check if box is reasonable
            if (width < 10 || height < 10 || width > canvas.cols || height > canvas.rows) {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Skipping invalid face target: x=%d, y=%d, w=%d, h=%d (canvas: %dx%d)",
                                                         node_name.c_str(),
                                                         i->x, i->y, i->width, i->height,
                                                         canvas.cols, canvas.rows));
                continue;
            }
            
            // Check aspect ratio (faces should be roughly square to slightly rectangular)
            float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
            if (aspect_ratio < 0.2f || aspect_ratio > 5.0f) {
                CVEDIX_WARN(cvedix_utils::string_format("[%s] Skipping face with extreme aspect ratio: %.2f (x=%d, y=%d, w=%d, h=%d)",
                                                         node_name.c_str(), aspect_ratio, x, y, width, height));
                continue;
            }
            
            cv::rectangle(canvas, cv::Rect(x, y, width, height), cv::Scalar(0, 255, 0), 2);
            
            CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Drew face box: x=%d, y=%d, w=%d, h=%d, score=%.4f (original: x=%d, y=%d, w=%d, h=%d)",
                                                      node_name.c_str(),
                                                      x, y, width, height, i->score,
                                                      i->x, i->y, i->width, i->height));

            // track_id
            if (i->track_id != -1) {
                auto id = std::to_string(i->track_id);
                cv::putText(canvas, id, cv::Point(i->x, i->y), 1, 1.5, cv::Scalar(0, 0, 255));
            }

            // just handle 5 keypoints
            if (i->key_points.size() >= 5) {
                // Validate and clamp keypoints before drawing
                for (size_t kp_idx = 0; kp_idx < 5 && kp_idx < i->key_points.size(); ++kp_idx) {
                    int kp_x = std::max(0, std::min(i->key_points[kp_idx].first, canvas.cols - 1));
                    int kp_y = std::max(0, std::min(i->key_points[kp_idx].second, canvas.rows - 1));
                    
                    // Check if keypoint is within reasonable distance from box center
                    int box_center_x = x + width / 2;
                    int box_center_y = y + height / 2;
                    float dist_from_center = std::sqrt(std::pow(kp_x - box_center_x, 2) + std::pow(kp_y - box_center_y, 2));
                    float max_dist = std::max(width, height) * 0.8f;  // Keypoints should be within 80% of box size
                    
                    if (dist_from_center > max_dist) {
                        CVEDIX_DEBUG(cvedix_utils::string_format("[%s] Keypoint %zu too far from box center: dist=%.1f, max=%.1f",
                                                                  node_name.c_str(), kp_idx, dist_from_center, max_dist));
                        continue;  // Skip this keypoint
                    }
                    
                    // Draw keypoints with different colors
                    cv::Scalar colors[] = {
                        cv::Scalar(255, 0, 0),    // right eye - red
                        cv::Scalar(0, 0, 255),    // left eye - blue
                        cv::Scalar(0, 255, 0),    // nose - green
                        cv::Scalar(255, 0, 255),  // right mouth corner - magenta
                        cv::Scalar(0, 255, 255)   // left mouth corner - cyan
                    };
                    cv::circle(canvas, cv::Point(kp_x, kp_y), 3, colors[kp_idx], 2);
                }
            }
        }

        return meta;
    }

    std::shared_ptr<cvedix_objects::cvedix_meta> cvedix_face_osd_node::handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) {
        return meta;
    }
}