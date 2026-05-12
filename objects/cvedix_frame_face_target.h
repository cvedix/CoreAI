/**
 * @file cvedix_frame_face_target.h
 * @brief Face detection/recognition target
 * 
 * Face bounding box, keypoints, embeddings, and recognition results.
 */

#pragma once
#include <vector>
#include <memory>
#include <string>
#include "shapes/cvedix_rect.h"

namespace cvedix_objects {
    /**
     * @brief Face detection target with recognition
     */
    class cvedix_frame_face_target
    {

    private:
        /* data */
    public:
        cvedix_frame_face_target(int x, 
                            int y, 
                            int width, 
                            int height, 
                            float score, 
                            std::vector<std::pair<int, int>> key_points = std::vector<std::pair<int, int>>(), 
                            std::vector<float> embeddings = std::vector<float>());
        ~cvedix_frame_face_target();

        // x of top left
        int x;
        // y of top left
        int y;
        // width of rect
        int width;
        // height of rect
        int height;

        // confidence
        float score;

        // feature vector created by face feature encoder inference nodes.
        // embeddings can be used for face recognize or other reid works.
        std::vector<float> embeddings;

        // key points (5 points or more)
        std::vector<std::pair<int, int>> key_points;

        // track id filled by cvedix_track_node (child class) if it exists.
        int track_id = -1;
        // cache of track rects in the previous frames, filled by track node if it exists. 
        // we can draw / analyse depend on these track rects later.
        std::vector<cvedix_objects::cvedix_rect> tracks;
        
        // Face recognition results (filled by face registration/recognition node)
        std::string identify = "";        // Recognized person name (empty or "Unknown" if not recognized)
        float identify_score = 0.0f;      // Recognition confidence score

        // ── Face Analysis Attributes (filled by cvedix_face_analysis_node) ──

        // Age prediction
        int age = -1;                     // Predicted age (-1 = not analyzed)

        // Gender prediction
        int gender = -1;                  // 0=Male, 1=Female, -1=unknown
        std::string gender_str = "";      // "Male" / "Female" / ""

        // Eye state
        int left_eye_state = -1;          // 0=Close, 1=Open, 2=Random, 3=Unknown, -1=not analyzed
        int right_eye_state = -1;         // 0=Close, 1=Open, 2=Random, 3=Unknown, -1=not analyzed

        // Head pose estimation (degrees)
        float yaw = 0.0f;                 // Left-right rotation
        float pitch = 0.0f;              // Up-down rotation
        float roll = 0.0f;               // Tilt rotation
        bool pose_valid = false;          // Whether pose was estimated

        // Anti-spoofing / Liveness detection
        int liveness_status = -1;         // 0=REAL, 1=SPOOF, 2=FUZZY, -1=not checked
        float liveness_clarity = 0.0f;    // Face clarity score from anti-spoofing
        float liveness_reality = 0.0f;    // Reality score from anti-spoofing

        // Mask detection
        bool wearing_mask = false;        // Whether face is wearing a mask
        float mask_score = 0.0f;          // Mask detection confidence
        std::shared_ptr<cvedix_frame_face_target> clone();

        // rect area of target
        cvedix_rect get_rect() const;
    };

}