/**
 * @file sample_output_helper.h
 * @brief Shared helper for samples: switch output mode via CLI args
 *
 * Supports 3 output modes selectable at runtime:
 *   --mode desktop   → cvedix_screen_des_node (requires X11/Wayland)
 *   --mode web       → cvedix_web_debug_des_node (browser dashboard, default)
 *   --mode rtmp      → cvedix_rtmp_des_node
 *
 * Additional args:
 *   --port <number>  → Web debug port (default: 9091)
 *   --rtmp <url>     → RTMP URL (default: rtmp://127.0.0.1/live/9000)
 *
 * Usage in sample:
 * @code
 *   #include "sample_output_helper.h"
 *
 *   int main(int argc, char** argv) {
 *       auto out_cfg = sample_helper::parse_output_args(argc, argv);
 *       // ... create pipeline nodes ...
 *       auto output = sample_helper::create_output(out_cfg, "des", 0, {file_src});
 *       output.des_node->attach_to({osd});
 *       // ... start pipeline ...
 *       sample_helper::wait_and_cleanup(file_src, out_cfg);
 *   }
 * @endcode
 */

#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cvedix/nodes/common/cvedix_des_node.h"
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

namespace sample_helper {

/// @brief Output mode enum
enum class OutputMode { DESKTOP, WEB, RTMP };

/// @brief Output configuration parsed from CLI args
struct OutputConfig {
    OutputMode mode = OutputMode::WEB;
    int web_port = 9091;
    std::string rtmp_url = "rtmp://127.0.0.1/live/9000";
};

/// @brief Result of create_output: destination node + optional analysis board
struct OutputResult {
    std::shared_ptr<cvedix_nodes::cvedix_des_node> des_node;
    /// @brief Analysis board (owned, for web mode lifetime management)
    std::unique_ptr<cvedix_utils::cvedix_analysis_board> board;
    /// @brief Source nodes saved for deferred board init (web mode)
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> _src_nodes;
};

/**
 * @brief Parse --mode, --port, --rtmp from command line
 * @note Skips unknown args silently so samples can have their own args
 */
inline OutputConfig parse_output_args(int argc, char** argv) {
    OutputConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "desktop") cfg.mode = OutputMode::DESKTOP;
            else if (mode == "web")     cfg.mode = OutputMode::WEB;
            else if (mode == "rtmp")    cfg.mode = OutputMode::RTMP;
        } else if (arg == "--port" && i + 1 < argc) {
            cfg.web_port = std::stoi(argv[++i]);
        } else if (arg == "--rtmp" && i + 1 < argc) {
            cfg.rtmp_url = argv[++i];
        }
    }
    return cfg;
}

/**
 * @brief Create destination node based on output config
 * 
 * NOTE: Does NOT create analysis_board yet (because pipe_checker inside
 * analysis_board requires fully-attached pipeline). Call init_board()
 * AFTER all attach_to() calls are done.
 * 
 * @param cfg Output configuration
 * @param name_prefix Name prefix for the des node
 * @param channel_index Channel index
 * @param src_nodes Source nodes for analysis board (web mode only, saved for init_board)
 * @return OutputResult with des_node (board is null until init_board)
 */
inline OutputResult create_output(
    const OutputConfig& cfg,
    const std::string& name_prefix,
    int channel_index,
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> src_nodes = {})
{
    OutputResult result;
    result._src_nodes = src_nodes;
    switch (cfg.mode) {
    case OutputMode::DESKTOP: {
        result.des_node = std::make_shared<cvedix_nodes::cvedix_screen_des_node>(
            name_prefix + "_screen", channel_index);
        break;
    }
    case OutputMode::WEB: {
        auto web = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(
            name_prefix + "_web", channel_index, cfg.web_port, nullptr, 50);
        result.des_node = web;
        break;
    }
    case OutputMode::RTMP: {
        result.des_node = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
            name_prefix + "_rtmp", channel_index, cfg.rtmp_url);
        break;
    }
    }
    return result;
}

/**
 * @brief Initialize analysis board for web mode (MUST be called AFTER pipeline is fully attached)
 * 
 * analysis_board constructor runs pipe_checker which validates pipeline topology,
 * so this must only be called when all nodes are connected via attach_to().
 * 
 * @param result OutputResult from create_output
 */
inline void init_board(OutputResult& result) {
    if (!result._src_nodes.empty()) {
        auto web = std::dynamic_pointer_cast<cvedix_nodes::cvedix_web_debug_des_node>(result.des_node);
        if (web) {
            result.board = std::make_unique<cvedix_utils::cvedix_analysis_board>(result._src_nodes);
            result.board->push_to_buffer(5);
            web->set_board(result.board.get());
        }
    }
    result._src_nodes.clear();  // free memory
}

/// @brief Get human-readable string for output mode
inline std::string mode_string(OutputMode mode) {
    switch (mode) {
    case OutputMode::DESKTOP: return "desktop (screen)";
    case OutputMode::WEB:     return "web (browser)";
    case OutputMode::RTMP:    return "rtmp";
    }
    return "unknown";
}

/// @brief Print output-related usage/help
inline void print_output_usage() {
    std::cout << "Output options:\n"
              << "  --mode <desktop|web|rtmp>  Output mode (default: web)\n"
              << "  --port <number>            Web debug port (default: 9091)\n"
              << "  --rtmp <url>               RTMP URL (default: rtmp://127.0.0.1/live/9000)\n";
}

/// @brief Print startup banner with output info
inline void print_output_info(const OutputConfig& cfg) {
    std::cout << "\n"
              << "╔════════════════════════════════════════════╗\n"
              << "║  Output: " << mode_string(cfg.mode);
    // pad to fixed width
    for (size_t i = mode_string(cfg.mode).size(); i < 33; ++i) std::cout << ' ';
    std::cout << "║\n";
    if (cfg.mode == OutputMode::WEB) {
        std::cout << "║  Open: http://localhost:" << cfg.web_port;
        std::string url = "http://localhost:" + std::to_string(cfg.web_port);
        for (size_t i = url.size(); i < 27; ++i) std::cout << ' ';
        std::cout << "║\n";
    } else if (cfg.mode == OutputMode::RTMP) {
        std::cout << "║  RTMP: " << cfg.rtmp_url;
        for (size_t i = cfg.rtmp_url.size(); i < 35; ++i) std::cout << ' ';
        std::cout << "║\n";
    }
    std::cout << "║  Press Enter to stop...                    ║\n"
              << "╚════════════════════════════════════════════╝\n"
              << std::endl;
}

}  // namespace sample_helper
