/**
 * @file cvedix_speed_violation_mllm_trigger_node.h
 * @brief Trigger MLLM analysis when speed violation detected
 * 
 * Monitors BA results for speed violations (>limit).
 * On violation, crops the vehicle and sends to MLLM for analysis.
 * 
 * @note Uses forward declarations to avoid llmlib/OpenSSL header pollution.
 *       This allows including this header alongside Eigen-dependent tracker headers.
 * 
 * @section prereq Prerequisites
 * - Compile with `-DCVEDIX_WITH_LLM`
 */

#pragma once

#ifdef CVEDIX_WITH_LLM

#include "cvedix/nodes/common/cvedix_node.h"
#include <memory>
#include <string>
#include <atomic>

namespace cvedix_nodes {

    /**
     * @brief Speed violation MLLM trigger node
     * 
     * Only invokes MLLM when speed_estimation BA result contains [VIOLATION].
     * Crops the violating vehicle, sends to LLM for detailed analysis.
     * Results stored in frame_meta->description and logged to file.
     */
    class cvedix_speed_violation_mllm_trigger_node : public cvedix_node {
    private:
        struct Impl;
        std::unique_ptr<Impl> pimpl;

    protected:
        std::shared_ptr<cvedix_objects::cvedix_meta> handle_frame_meta(
            std::shared_ptr<cvedix_objects::cvedix_frame_meta> meta) override;

    public:
        /**
         * @param node_name      Node identifier
         * @param model_name     LLM model (e.g. "minicpm-v")
         * @param prompt         Analysis prompt
         * @param api_url        LLM API URL (e.g. "http://localhost:11434")
         * @param log_path       Path to violation log file
         * @param cooldown_frames Minimum frames between analyses of same track
         */
        cvedix_speed_violation_mllm_trigger_node(
            const std::string& node_name,
            const std::string& model_name,
            const std::string& prompt,
            const std::string& api_url,
            const std::string& log_path = "./output/speed_violations.log",
            int cooldown_frames = 300);
        
        ~cvedix_speed_violation_mllm_trigger_node();

        int get_violation_count() const;
    };

} // namespace cvedix_nodes

#endif
