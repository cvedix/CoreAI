#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
* ## N-1-N sample ##
* 2 video input and merge into 1 branch automatically for 1 infer task,
* then resume to 2 branches for outputs again.
*
* Usage:
*   ./N-1-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*
* In web mode, pipe 0 uses port, pipe 1 uses port+1.
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::WARN);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 0.6);
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 1, "./cvedix_data/video/face2.mp4", 0.6);
    auto yunet_face_detector = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");

    auto split = std::make_shared<cvedix_nodes::cvedix_split_node>("split", true);  // split by channel index

    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    auto output_0 = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0, file_src_1});

    // pipe 1 - use port+1 for web mode
    auto out_cfg_1 = out_cfg;
    out_cfg_1.web_port = out_cfg.web_port + 1;
    auto osd_1 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_1");
    auto output_1 = sample_helper::create_output(out_cfg_1, "des_1", 1, {file_src_0, file_src_1});

    // construct pipeline
    yunet_face_detector->attach_to({file_src_0, file_src_1});
    split->attach_to({yunet_face_detector});

    // split by cvedix_split_node
    osd_0->attach_to({split});
    osd_1->attach_to({split});

    output_0.des_node->attach_to({osd_0});
    output_1.des_node->attach_to({osd_1});

    file_src_0->start();
    file_src_1->start();

    sample_helper::init_board(output_0);
    sample_helper::init_board(output_1);

    sample_helper::print_output_info(out_cfg);
    if (out_cfg.mode == sample_helper::OutputMode::WEB) {
        std::cout << "  Pipe 1: http://localhost:" << out_cfg_1.web_port << std::endl;
    }

    std::string wait;
    std::getline(std::cin, wait);
    file_src_0->detach_recursively();
}
