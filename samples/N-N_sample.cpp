#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_face_detector_node.h"
#include "cvedix/nodes/osd/cvedix_osd_node.h"

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "sample_output_helper.h"

/*
* ## N-N sample ##
* multi pipe exist separately and each pipe is 1-1-1
* (can be any structure like 1-1-N, 1-N-N)
*
* Usage:
*   ./N-N_sample [--mode desktop|web|rtmp] [--port 9091] [--rtmp url]
*
* In web mode, pipe 0 uses port, pipe 1 uses port+1.
*/

int main(int argc, char** argv) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_LOGGER_INIT();

    auto out_cfg = sample_helper::parse_output_args(argc, argv);

    // create nodes - pipe 0
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, "./cvedix_data/video/face.mp4", 0.6);
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_0", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_0");
    auto output_0 = sample_helper::create_output(out_cfg, "des_0", 0, {file_src_0});

    // create nodes - pipe 1 (port+1 for web mode)
    auto out_cfg_1 = out_cfg;
    out_cfg_1.web_port = out_cfg.web_port + 1;
    auto file_src_1 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_1", 0, "./cvedix_data/video/face2.mp4");
    auto yunet_face_detector_1 = std::make_shared<cvedix_nodes::cvedix_face_detector_node>("yunet_face_detector_1", "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx");
    auto osd_1 = std::make_shared<cvedix_nodes::cvedix_osd_node>("osd_1");
    auto output_1 = sample_helper::create_output(out_cfg_1, "des_1", 0, {file_src_1});

    // construct pipeline
    // pipe 0
    yunet_face_detector_0->attach_to({file_src_0});
    osd_0->attach_to({yunet_face_detector_0});
    output_0.des_node->attach_to({osd_0});

    // pipe 1
    yunet_face_detector_1->attach_to({file_src_1});
    osd_1->attach_to({yunet_face_detector_1});
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
    file_src_1->detach_recursively();
}
