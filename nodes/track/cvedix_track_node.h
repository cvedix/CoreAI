/**
 * @file cvedix_track_node.h
 * @brief Base class for object tracking nodes in Core AI Runtime
 * 
 * This file defines the cvedix_track_node base class for multi-object tracking (MOT).
 * Tracking nodes assign persistent IDs to detected objects across video frames.
 * 
 * @section track_overview Overview
 * Tracking nodes process detection results and:
 * - Assign unique track IDs to each detected object
 * - Maintain track history across frames
 * - Handle object appearance/disappearance
 * 
 * @section track_algorithms Supported Algorithms
 * - **SORT**: Simple Online Realtime Tracking (cvedix_sort_track_node)
 * - **DeepSORT**: SORT with appearance features (cvedix_dsort_track_node)
 * 
 * @section track_multichannel Multi-Channel Support
 * Track nodes support multiple video channels simultaneously.
 * Each channel maintains its own independent set of tracks.
 * 
 * @section track_usage Usage Example
 * @code
 * auto tracker = std::make_shared<cvedix_sort_track_node>(
 *     "tracker",
 *     cvedix_track_for::NORMAL  // Track detected objects
 * );
 * tracker->attach_to({detector_node});
 * osd_node->attach_to({tracker});
 * @endcode
 * 
 * @see cvedix_sort_track_node SORT implementation
 * @see cvedix_dsort_track_node DeepSORT implementation
 */

#pragma once

#include <map>
#include <assert.h>
#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {

    /**
     * @brief Specifies which target type the tracker should process
     */
    enum class cvedix_track_for {
        NORMAL = 1,    ///< cvedix_frame_target - general object tracking
        FACE = 2       ///< cvedix_frame_face_target - face tracking
    };

    /**
     * @brief Base class for multi-object tracking nodes
     * 
     * Provides infrastructure for tracking detected objects across frames,
     * assigning persistent IDs, and maintaining track histories.
     * 
     * @section track_pipeline Processing Pipeline
     * 1. **preprocess()**: Extract bounding boxes and features from frame_meta
     * 2. **track()**: Run tracking algorithm (pure virtual - implement in derived)
     * 3. **postprocess()**: Write track IDs back to frame_meta
     * 
     * @note This is an abstract base class. Use cvedix_sort_track_node or
     *       cvedix_dsort_track_node.
     * 
     * @see cvedix_node Base class
     */
    class cvedix_track_node: public cvedix_node {
    private:
        /// @brief Target type to track
        cvedix_track_for track_for = cvedix_track_for::NORMAL;
        
        /// @brief Track history: channel → track_id → rect history
        std::map<int, std::map<int, std::vector<cvedix_objects::cvedix_rect>>> all_tracks_by_id;

        /// @brief Last frame index for each track: channel → track_id → frame_index
        std::map<int, std::map<int, int>> all_last_tracked_frame_indexes;

        /// @brief Maximum frames before a lost track is removed
        const int max_allowed_disappear_frames = 25;

    protected:
        /**
         * @brief Handle incoming frame meta
         * @param meta Frame meta with detected targets
         * @return Processed meta with track IDs assigned
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override final;

        /**
         * @brief Handle control meta (pass through)
         * @param meta Control meta
         * @return The same meta passed through
         */
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override final;

        /**
         * @brief Extract tracking data from frame meta
         * 
         * @param frame_meta Input frame meta with targets
         * @param[out] target_rects Bounding boxes of detected objects
         * @param[out] target_embeddings Feature embeddings (optional, for DeepSORT)
         */
        void preprocess(std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta, 
                        std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        std::vector<std::vector<float>>& target_embeddings);
        
        /**
         * @brief Execute tracking algorithm
         * 
         * Pure virtual method - must be implemented by derived classes.
         * Associates current detections with existing tracks.
         * 
         * @param channel_index Video channel index
         * @param frame_meta Current frame meta
         * @param target_rects Bounding boxes to track
         * @param target_embeddings Feature embeddings (optional)
         * @param[out] track_ids Assigned track IDs (same order as input rects)
         */
        virtual void track(int channel_index, 
                        const std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta, 
                        const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        const std::vector<std::vector<float>>& target_embeddings, 
                        std::vector<int>& track_ids) = 0;

        /**
         * @brief Write track results back to frame meta
         * 
         * Updates targets with assigned track IDs and track histories.
         * 
         * @param frame_meta Frame meta to update
         * @param target_rects Original bounding boxes
         * @param target_embeddings Feature embeddings
         * @param track_ids Assigned track IDs
         */
        void postprocess(std::shared_ptr<cvedix_objects::cvedix_frame_meta> frame_meta, 
                        const std::vector<cvedix_objects::cvedix_rect>& target_rects, 
                        const std::vector<std::vector<float>>& target_embeddings, 
                        const std::vector<int>& track_ids);

    public:
        /**
         * @brief Constructor
         * 
         * @param node_name Unique node identifier
         * @param track_for Target type to track (default: NORMAL)
         */
        cvedix_track_node(std::string node_name, cvedix_track_for track_for = cvedix_track_for::NORMAL);

        /// @brief Virtual destructor
        virtual ~cvedix_track_node();
    };
}