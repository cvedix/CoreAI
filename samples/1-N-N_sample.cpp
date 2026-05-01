#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
* ## 1-N-N sample ##
* 1 video input and then split into 2 branches for different infer tasks,
* then 2 total outputs (no need to sync in such situations).
*
* Usage:
*   ./1-N-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*
* In web mode, branch A uses port, branch B uses port+1.
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 1.0);
    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", false, true);

    // branch a
    auto yunet_face_detector_a = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_a", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");
    auto osd_a = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_a");
    auto output_a = sample_helper::create_output(out_cfg, "des_a", 0, {file_src_0});

    // branch b - use port+1 for web mode
    auto out_cfg_b = out_cfg;
    out_cfg_b.web_port = out_cfg.web_port + 1;
    auto yunet_face_detector_b = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_b", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");
    auto osd_b = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_b");
    auto output_b = sample_helper::create_output(out_cfg_b, "des_b", 0, {file_src_0});

    // construct pipeline
    split->attach_to({file_src_0});

    // branch a
    yunet_face_detector_a->attach_to({split});
    osd_a->attach_to({yunet_face_detector_a});
    output_a.des_node->attach_to({osd_a});

    // branch b
    yunet_face_detector_b->attach_to({split});
    osd_b->attach_to({yunet_face_detector_b});
    output_b.des_node->attach_to({osd_b});

    file_src_0->start();

    // init boards AFTER pipeline is fully attached (pipe_checker runs in board constructor)
    sample_helper::init_board(output_a);
    sample_helper::init_board(output_b);

    sample_helper::print_output_info(out_cfg);
    if (out_cfg.mode == sample_helper::OutputMode::WEB) {
        std::cout << "  Branch B: http://localhost:" << out_cfg_b.web_port << std::endl;
    }

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
