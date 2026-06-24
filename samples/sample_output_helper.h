#pragma once

#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "cvedix/nodes/common/cvedix_des_node.h"
#include "cvedix/nodes/common/cvedix_node.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_web_debug_des_node.h"
#ifdef CVEDIX_WITH_GSTREAMER
#include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#endif
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"

namespace sample_helper {

enum class OutputMode {
    DESKTOP,
    WEB,
    RTMP,
};

struct OutputConfig {
    OutputMode mode = OutputMode::WEB;
    int web_port = 9091;
    std::string rtmp_url = "rtmp://127.0.0.1/live/9000";
};

struct OutputHandle {
    OutputConfig config;
    std::shared_ptr<cvedix_nodes::cvedix_des_node> des_node;
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> board_sources;
    std::shared_ptr<cvedix_utils::cvedix_analysis_board> board;
};

inline std::string normalize_arg(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline OutputConfig parse_output_args(int argc, char** argv) {
    OutputConfig cfg;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            const std::string mode = normalize_arg(argv[++i]);
            if (mode == "desktop" || mode == "screen") {
                cfg.mode = OutputMode::DESKTOP;
            } else if (mode == "web" || mode == "browser") {
                cfg.mode = OutputMode::WEB;
            } else if (mode == "rtmp") {
                cfg.mode = OutputMode::RTMP;
            }
        } else if (arg == "--port" && i + 1 < argc) {
            cfg.web_port = std::stoi(argv[++i]);
        } else if (arg == "--rtmp" && i + 1 < argc) {
            cfg.rtmp_url = argv[++i];
        }
    }
    return cfg;
}

inline OutputHandle create_output(
    const OutputConfig& cfg,
    const std::string& node_name,
    int channel_index,
    const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& board_sources) {

    OutputHandle output;
    output.config = cfg;
    output.board_sources = board_sources;

    switch (cfg.mode) {
        case OutputMode::DESKTOP:
            output.des_node = std::make_shared<cvedix_nodes::cvedix_screen_des_node>(node_name, channel_index, true);
            break;
        case OutputMode::WEB:
            output.des_node = std::make_shared<cvedix_nodes::cvedix_web_debug_des_node>(node_name + "_web", channel_index, cfg.web_port, nullptr, 70);
            break;
        case OutputMode::RTMP:
#ifdef CVEDIX_WITH_GSTREAMER
            output.des_node = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(node_name + "_rtmp", channel_index, cfg.rtmp_url, cvedix_objects::cvedix_size{}, 1024, true);
#else
            throw std::runtime_error("RTMP output requires CVEDIX_WITH_GSTREAMER");
#endif
            break;
    }

    return output;
}

inline void init_board(OutputHandle& output) {
    if (output.board_sources.empty()) {
        return;
    }

    output.board = std::make_shared<cvedix_utils::cvedix_analysis_board>(output.board_sources);
    if (output.config.mode == OutputMode::WEB) {
        output.board->push_to_buffer(3);
        auto web_des = std::dynamic_pointer_cast<cvedix_nodes::cvedix_web_debug_des_node>(output.des_node);
        if (web_des) {
            web_des->set_board(output.board.get());
        }
    }
}

inline void print_output_info(const OutputConfig& cfg) {
    switch (cfg.mode) {
        case OutputMode::DESKTOP:
            std::cout << "\n╔════════════════════════════════════════════╗\n"
                      << "║  Output: desktop (screen)                ║\n"
                      << "║  Press Enter to stop...                  ║\n"
                      << "╚════════════════════════════════════════════╝\n\n";
            break;
        case OutputMode::WEB:
            std::cout << "\n╔════════════════════════════════════════════╗\n"
                      << "║  Output: web (browser)                   ║\n"
                      << "║  Open: http://localhost:" << cfg.web_port;
            if (cfg.web_port < 10000) {
                std::cout << "      ";
            }
            std::cout << "║\n"
                      << "║  Press Enter to stop...                  ║\n"
                      << "╚════════════════════════════════════════════╝\n\n";
            break;
        case OutputMode::RTMP:
            std::cout << "\n╔════════════════════════════════════════════╗\n"
                      << "║  Output: rtmp                            ║\n"
                      << "║  URL: " << cfg.rtmp_url << "\n"
                      << "║  Press Enter to stop...                  ║\n"
                      << "╚════════════════════════════════════════════╝\n\n";
            break;
    }
}

} // namespace sample_helper
