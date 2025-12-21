/**
 * @file cvedix_sync_node.h
 * @brief Synchronization node for merging parallel pipeline branches
 * 
 * Merges data from parallel branches that were split by cvedix_split_node.
 * 
 * @section sync_modes Sync Modes
 * - MERGE: Combines target collections (for primary infer results)
 * - UPDATE: Updates target properties (for secondary infer results)
 * 
 * @see cvedix_split_node For splitting pipeline
 */

#pragma once

#include "cvedix/nodes/common/cvedix_node.h"

namespace cvedix_nodes {
    /**
     * @brief Sync mode for merging parallel branches
     */
    enum class cvedix_sync_mode {
        MERGE = 0,   ///< Merge target collections (primary infer)
        UPDATE = 1   ///< Update target properties (secondary infer)
    };

    /**
     * @brief Synchronization node for parallel branch merging
     * 
     * Syncs frame_meta from parallel branches by frame index/channel.
     */
    class cvedix_sync_node: public cvedix_node
    {

    private:
        // do sync work
        void sync(std::shared_ptr<cvedix_objects::cvedix_frame_meta> des, std::shared_ptr<cvedix_objects::cvedix_frame_meta> src);
        // sync mode
        cvedix_sync_mode mode = cvedix_sync_mode::MERGE;
        // push to downstream directly after waiting a period of time(milliseconds), because we can not wait infinitely
        int timeout = 40;  // wait for 40ms for each frame since 25fps is normal for video stream

        /* multi-channel supported*/
        std::map<int, int> all_indexs_last_syned;
        std::map<int, std::string> all_control_uids_last_synced;
        std::map<int, std::vector<std::shared_ptr<cvedix_objects::cvedix_meta>>> all_meta_waiting_for_sync;
    protected:
        // re-implementation
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;
        // re-implementation
        virtual std::shared_ptr<cvedix_objects::cvedix_meta> handle_control_meta(std::shared_ptr<cvedix_objects::cvedix_control_meta> meta) override;
    public:
        cvedix_sync_node(std::string node_name, cvedix_sync_mode mode = cvedix_sync_mode::MERGE, int timeout = 40);
        ~cvedix_sync_node();
    };
}