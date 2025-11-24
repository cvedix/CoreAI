#include "cvedix_json_enhanced_console_broker_node.h"
#include <opencv2/imgcodecs.hpp>
#include <vector>
#include <algorithm>
#include "cereal_archive/cvedix_objects_cereal_archive.h"

// Enhanced target structure for serialization
namespace cvedix_objects {
    struct enhanced_target_info {
        // Original target info
        int x, y, width, height;
        int primary_class_id;
        float primary_score;
        std::string primary_label;
        int frame_index;
        int channel_index;
        int track_id;
        
        // Base64 encoded crop image
        std::string crop_base64;
        
        // Bounding box in different formats
        struct bbox_info {
            int x1, y1, x2, y2;
            int center_x, center_y;
            
            template<typename Archive>
            void serialize(Archive& archive) {
                archive(cereal::make_nvp("x1", x1),
                        cereal::make_nvp("y1", y1),
                        cereal::make_nvp("x2", x2),
                        cereal::make_nvp("y2", y2),
                        cereal::make_nvp("center_x", center_x),
                        cereal::make_nvp("center_y", center_y));
            }
        } bbox;
        
        // Secondary information
        std::vector<int> secondary_class_ids;
        std::vector<float> secondary_scores;
        std::vector<std::string> secondary_labels;
        
        // Additional info
        size_t embeddings_size;
        size_t sub_targets_count;
        
        template<typename Archive>
        void serialize(Archive& archive) {
            archive(cereal::make_nvp("x", x),
                    cereal::make_nvp("y", y),
                    cereal::make_nvp("width", width),
                    cereal::make_nvp("height", height),
                    cereal::make_nvp("primary_class_id", primary_class_id),
                    cereal::make_nvp("primary_score", primary_score),
                    cereal::make_nvp("primary_label", primary_label),
                    cereal::make_nvp("frame_index", frame_index),
                    cereal::make_nvp("channel_index", channel_index),
                    cereal::make_nvp("track_id", track_id),
                    cereal::make_nvp("crop_base64", crop_base64),
                    cereal::make_nvp("bbox", bbox),
                    cereal::make_nvp("secondary_class_ids", secondary_class_ids),
                    cereal::make_nvp("secondary_scores", secondary_scores),
                    cereal::make_nvp("secondary_labels", secondary_labels),
                    cereal::make_nvp("embeddings_size", embeddings_size),
                    cereal::make_nvp("sub_targets_count", sub_targets_count));
        }
    };
    
    struct enhanced_face_target_info {
        int x, y, width, height;
        float score;
        int track_id;
        std::string crop_base64;
        
        struct bbox_info {
            int x1, y1, x2, y2;
            int center_x, center_y;
            
            template<typename Archive>
            void serialize(Archive& archive) {
                archive(cereal::make_nvp("x1", x1),
                        cereal::make_nvp("y1", y1),
                        cereal::make_nvp("x2", x2),
                        cereal::make_nvp("y2", y2),
                        cereal::make_nvp("center_x", center_x),
                        cereal::make_nvp("center_y", center_y));
            }
        } bbox;
        
        std::vector<std::pair<int, int>> key_points;
        size_t embeddings_size;
        
        template<typename Archive>
        void serialize(Archive& archive) {
            archive(cereal::make_nvp("x", x),
                    cereal::make_nvp("y", y),
                    cereal::make_nvp("width", width),
                    cereal::make_nvp("height", height),
                    cereal::make_nvp("score", score),
                    cereal::make_nvp("track_id", track_id),
                    cereal::make_nvp("crop_base64", crop_base64),
                    cereal::make_nvp("bbox", bbox),
                    cereal::make_nvp("key_points", key_points),
                    cereal::make_nvp("embeddings_size", embeddings_size));
        }
    };
}

namespace cvedix_nodes {
    
    cvedix_json_enhanced_console_broker_node::cvedix_json_enhanced_console_broker_node(
        std::string node_name, 
        cvedix_broke_for broke_for, 
        int broking_cache_warn_threshold, 
        int broking_cache_ignore_threshold,
        bool encode_full_frame):
        cvedix_json_console_broker_node(node_name, broke_for, broking_cache_warn_threshold, broking_cache_ignore_threshold),
        encode_full_frame(encode_full_frame) {
        // Note: initialized() is already called by base class cvedix_json_console_broker_node
        // Do not call it again here to avoid thread assignment conflict
    }
    
    cvedix_json_enhanced_console_broker_node::~cvedix_json_enhanced_console_broker_node() {
        // Note: deinitialized() and stop_broking() are already called by base class
        // Do not call them again here
    }
    
    std::string cvedix_json_enhanced_console_broker_node::mat_to_base64(const cv::Mat& img, const std::string& ext) {
        if (img.empty()) {
            return "";
        }
        
        std::vector<uchar> buf;
        cv::imencode(ext, img, buf);
        std::string encoded = base64_encode(buf.data(), buf.size());
        return encoded;
    }
    
    cv::Mat cvedix_json_enhanced_console_broker_node::crop_image(const cv::Mat& frame, int x, int y, int width, int height) {
        if (frame.empty()) {
            return cv::Mat();
        }
        
        // Ensure coordinates are within frame bounds
        int x1 = std::max(0, x);
        int y1 = std::max(0, y);
        int x2 = std::min(frame.cols, x + width);
        int y2 = std::min(frame.rows, y + height);
        
        if (x2 <= x1 || y2 <= y1) {
            return cv::Mat();
        }
        
        cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
        return frame(roi).clone();
    }
    
    void cvedix_json_enhanced_console_broker_node::format_msg(const std::shared_ptr<cvedix_objects::cvedix_frame_meta>& meta, std::string& msg) {
        // Serialize objects to JSON by cereal with enhanced information
        try {
            std::stringstream msg_stream;
            {
                cereal::JSONOutputArchive json_archive(msg_stream);
                
                // Global values
                json_archive(cereal::make_nvp("channel_index", meta->channel_index),
                            cereal::make_nvp("frame_index", meta->frame_index),
                            cereal::make_nvp("width", meta->frame.cols),
                            cereal::make_nvp("height", meta->frame.rows),
                            cereal::make_nvp("fps", meta->fps),
                            cereal::make_nvp("broke_for", broke_fors.at(broke_for)));
                
                // Encode full frame to base64 (only if enabled and frame is valid)
                // Note: Encoding full frame can be memory intensive, so it's disabled by default
                std::string frame_base64 = "";
                if (encode_full_frame && !meta->frame.empty() && meta->frame.cols > 0 && meta->frame.rows > 0) {
                    try {
                        frame_base64 = mat_to_base64(meta->frame, ".jpg");
                    } catch (...) {
                        // If encoding fails, leave it empty
                        frame_base64 = "";
                    }
                }
                json_archive(cereal::make_nvp("frame_base64", frame_base64));
            
            // Serialize values according to broke_for
            if (broke_for == cvedix_broke_for::NORMAL) {
                json_archive(cereal::make_nvp("target_size", meta->targets.size()));
                
                // Create enhanced targets array with base64 crop images
                std::vector<cvedix_objects::enhanced_target_info> enhanced_targets;
                
                for (const auto& target : meta->targets) {
                    cvedix_objects::enhanced_target_info enhanced_target;
                    
                    // Copy basic info
                    enhanced_target.x = target->x;
                    enhanced_target.y = target->y;
                    enhanced_target.width = target->width;
                    enhanced_target.height = target->height;
                    enhanced_target.primary_class_id = target->primary_class_id;
                    enhanced_target.primary_score = target->primary_score;
                    enhanced_target.primary_label = target->primary_label;
                    enhanced_target.frame_index = target->frame_index;
                    enhanced_target.channel_index = target->channel_index;
                    enhanced_target.track_id = target->track_id;
                    
                    // Crop and encode target image
                    try {
                        cv::Mat cropped = crop_image(meta->frame, target->x, target->y, target->width, target->height);
                        if (!cropped.empty()) {
                            enhanced_target.crop_base64 = mat_to_base64(cropped, ".jpg");
                        } else {
                            enhanced_target.crop_base64 = "";
                        }
                    } catch (...) {
                        enhanced_target.crop_base64 = "";
                    }
                    
                    // Bounding box info
                    enhanced_target.bbox.x1 = target->x;
                    enhanced_target.bbox.y1 = target->y;
                    enhanced_target.bbox.x2 = target->x + target->width;
                    enhanced_target.bbox.y2 = target->y + target->height;
                    enhanced_target.bbox.center_x = target->x + target->width / 2;
                    enhanced_target.bbox.center_y = target->y + target->height / 2;
                    
                    // Secondary information
                    enhanced_target.secondary_class_ids = target->secondary_class_ids;
                    enhanced_target.secondary_scores = target->secondary_scores;
                    enhanced_target.secondary_labels = target->secondary_labels;
                    
                    // Additional info
                    enhanced_target.embeddings_size = target->embeddings.size();
                    enhanced_target.sub_targets_count = target->sub_targets.size();
                    
                    enhanced_targets.push_back(enhanced_target);
                }
                
                json_archive(cereal::make_nvp("targets", enhanced_targets));
            }
            else if (broke_for == cvedix_broke_for::FACE) {
                json_archive(cereal::make_nvp("face_target_size", meta->face_targets.size()));
                
                // Create enhanced face targets array
                std::vector<cvedix_objects::enhanced_face_target_info> enhanced_face_targets;
                
                for (const auto& face_target : meta->face_targets) {
                    cvedix_objects::enhanced_face_target_info enhanced_target;
                    
                    // Copy basic info
                    enhanced_target.x = face_target->x;
                    enhanced_target.y = face_target->y;
                    enhanced_target.width = face_target->width;
                    enhanced_target.height = face_target->height;
                    enhanced_target.score = face_target->score;
                    enhanced_target.track_id = face_target->track_id;
                    
                    // Crop and encode face image
                    try {
                        cv::Mat cropped = crop_image(meta->frame, face_target->x, face_target->y, face_target->width, face_target->height);
                        if (!cropped.empty()) {
                            enhanced_target.crop_base64 = mat_to_base64(cropped, ".jpg");
                        } else {
                            enhanced_target.crop_base64 = "";
                        }
                    } catch (...) {
                        enhanced_target.crop_base64 = "";
                    }
                    
                    // Bounding box info
                    enhanced_target.bbox.x1 = face_target->x;
                    enhanced_target.bbox.y1 = face_target->y;
                    enhanced_target.bbox.x2 = face_target->x + face_target->width;
                    enhanced_target.bbox.y2 = face_target->y + face_target->height;
                    enhanced_target.bbox.center_x = face_target->x + face_target->width / 2;
                    enhanced_target.bbox.center_y = face_target->y + face_target->height / 2;
                    
                    // Key points
                    enhanced_target.key_points = face_target->key_points;
                    
                    // Embeddings size
                    enhanced_target.embeddings_size = face_target->embeddings.size();
                    
                    enhanced_face_targets.push_back(enhanced_target);
                }
                
                json_archive(cereal::make_nvp("face_targets", enhanced_face_targets));
            }
            else if (broke_for == cvedix_broke_for::TEXT) {
                json_archive(cereal::make_nvp("text_target_size", meta->text_targets.size()),
                            cereal::make_nvp("text_targets", meta->text_targets));
            }
            else {
                throw "invalid broke_for!";
            }
            } // flush
            
            msg = msg_stream.str();
        } catch (const std::exception& e) {
            // If serialization fails, return empty message or fallback to parent implementation
            msg = "";
            // Optionally log the error
            // std::cerr << "Error in enhanced broker format_msg: " << e.what() << std::endl;
        } catch (...) {
            // Catch any other exceptions
            msg = "";
        }
    }
}

